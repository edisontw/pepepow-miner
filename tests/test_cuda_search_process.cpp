#include "pepepow/cuda_search_process.hpp"

#include <array>
#include <chrono>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" void hoohashv110(const void*, std::size_t, std::uint8_t[32]);

namespace {

bool decode_hex(std::string_view input, std::uint8_t* output, std::size_t size) {
  if (input.size() != size * 2) return false;
  const auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0; index < size; ++index) {
    const auto high = nibble(input[index * 2]);
    const auto low = nibble(input[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

std::string_view argument_value(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) == name) return argv[index + 1];
  }
  return {};
}

bool parse_u32(std::string_view input, std::uint32_t& output) {
  const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), output);
  return error == std::errc{} && end == input.data() + input.size();
}

int mock_child(int argc, char** argv, bool wait_for_cancel, bool corrupt_digest) {
  pepepow::Header header{};
  pepepow::Digest target{};
  std::uint32_t start = 0;
  std::uint32_t count = 0;
  std::uint32_t batch_size = 0;
  std::uint32_t timeout_ms = 0;
  const auto header_hex = argument_value(argc, argv, "--header-hex");
  const auto target_hex = argument_value(argc, argv, "--target-hex");
  const auto start_text = argument_value(argc, argv, "--nonce-start");
  const auto count_text = argument_value(argc, argv, "--nonce-count");
  const auto batch_text = argument_value(argc, argv, "--batch-size");
  const auto timeout_text = argument_value(argc, argv, "--timeout-ms");
  const auto cancel_path = argument_value(argc, argv, "--cancel-file");
  if (!decode_hex(header_hex, header.data(), header.size()) ||
      !decode_hex(target_hex, target.data(), target.size()) ||
      !parse_u32(start_text, start) || !parse_u32(count_text, count) ||
      !parse_u32(batch_text, batch_size) || !parse_u32(timeout_text, timeout_ms) ||
      cancel_path.empty())
    return 64;

  const auto emit = [&](std::string_view status, std::string_view stop_reason,
                        std::uint32_t processed, std::string_view candidates) {
    const std::uint64_t batch_count = processed == 0 ? 0 :
        (static_cast<std::uint64_t>(processed) + batch_size - 1) / batch_size;
    std::cout << "{\"schema_version\":1,\"status\":\""
              << (status == "cancelled" ? "cancelled" : "ok")
              << "\",\"engine\":\"cuda_hoohash_v110_bounded_search\","
              << "\"search_status\":\"" << status << "\",\"stop_reason\":";
    if (stop_reason.empty()) std::cout << "null";
    else std::cout << '\"' << stop_reason << '\"';
    std::cout << ",\"nonce_start\":" << start << ",\"nonce_count\":" << count
              << ",\"processed_count\":" << processed
              << ",\"next_nonce\":" << static_cast<std::uint64_t>(start) + processed
              << ",\"batch_size\":" << batch_size << ",\"batch_count\":" << batch_count
              << ",\"target_encoding\":\"little_endian_uint256\","
              << "\"cpu_recheck\":\"pinned_core_full_digest_and_target\","
              << "\"candidates\":" << candidates
              << ",\"device\":{\"index\":0,\"name\":\"CPU contract mock\","
              << "\"compute_capability\":\"0.0\"},"
              << "\"cuda_versions\":{\"driver_api\":0,\"runtime_api\":0,"
              << "\"toolkit_build\":\"mock\"},\"error\":null}\n" << std::flush;
  };

  if (wait_for_cancel) {
    const auto stop_at = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(timeout_ms + 3000U);
    while (!std::filesystem::exists(std::filesystem::path(cancel_path)) &&
           std::chrono::steady_clock::now() < stop_at)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!std::filesystem::exists(std::filesystem::path(cancel_path))) return 65;
    emit("cancelled", "cancel_file", 0, "[]");
    return 130;
  }

  pepepow_header_set_nonce(header.data(), header.size(), start);
  pepepow::Digest digest{};
  hoohashv110(header.data(), header.size(), digest.data());
  if (corrupt_digest) digest.back() ^= 0x80U;
  const auto digest_hex = pepepow::detail::encode_hex(digest.data(), digest.size());
  const std::string candidate = "[{\"nonce\":" + std::to_string(start) +
      ",\"digest_hex\":\"" + digest_hex + "\"}]";
  emit("found", {}, 1, candidate);
  return 0;
}

std::vector<std::filesystem::path> temporary_entries() {
  std::vector<std::filesystem::path> found;
  std::error_code error;
  const auto root = std::filesystem::temp_directory_path(error);
  if (error) return found;
  for (std::filesystem::directory_iterator it(root, error), end; !error && it != end;
       it.increment(error)) {
    if (it->path().filename().string().starts_with("pepepow-cuda-search-"))
      found.push_back(it->path());
  }
  return found;
}

int run_contract_tests(const char* executable) {
  pepepow::CpuWorkerPool cpu(1);
  pepepow::CudaSearchProcessOptions options;
  options.engine = std::filesystem::absolute(executable);
  options.cpu_oracle = "mock-pinned-core-oracle";
  options.engine_prefix_arguments = {"--mock-child-found"};
  options.timeout_ms = 1000;

  pepepow::Header header{};
  header[0] = 0x71;
  pepepow::Digest easy_target{};
  easy_target.fill(0xffU);
  const auto before = temporary_entries();
  auto result = pepepow::search_cuda_with_process(cpu, options, header, easy_target,
      UINT32_MAX, 1, pepepow::SearchClock::now() + std::chrono::seconds(3));
  if (result.status != pepepow::SearchStatus::Found || !result.candidate ||
      result.candidate->nonce != UINT32_MAX || result.hashes != 1) {
    std::cerr << "candidate launch/full-digest CPU recheck failed\n";
    return 1;
  }
  const auto after_found = temporary_entries();
  if (after_found != before) {
    std::cerr << "candidate launch left a temporary process directory\n";
    return 1;
  }

  options.engine_prefix_arguments = {"--mock-child-corrupt"};
  result = pepepow::search_cuda_with_process(cpu, options, header, easy_target,
      17, 1, pepepow::SearchClock::now() + std::chrono::seconds(3));
  if (result.status != pepepow::SearchStatus::Invalid || result.candidate) {
    std::cerr << "last-byte digest mutation was not rejected\n";
    return 1;
  }

  options.engine_prefix_arguments = {"--mock-child-found"};
  pepepow::Digest impossible_target{};
  result = pepepow::search_cuda_with_process(cpu, options, header, impossible_target,
      19, 1, pepepow::SearchClock::now() + std::chrono::seconds(3));
  if (result.status != pepepow::SearchStatus::Invalid || result.candidate) {
    std::cerr << "CPU target recheck accepted a below-target mock candidate\n";
    return 1;
  }

  options.engine_prefix_arguments = {"--mock-child-cancel"};
  std::atomic_bool cancel{false};
  std::jthread cancel_request([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    cancel.store(true, std::memory_order_release);
  });
  const auto cancellation_started = std::chrono::steady_clock::now();
  result = pepepow::search_cuda_with_process(cpu, options, header, easy_target,
      4, 100, pepepow::SearchClock::now() + std::chrono::seconds(3), &cancel);
  cancel_request.join();
  const auto stop_delay = std::chrono::steady_clock::now() - cancellation_started;
  if (result.status != pepepow::SearchStatus::Cancelled || result.candidate ||
      stop_delay > std::chrono::seconds(2)) {
    std::cerr << "child cancellation did not complete cooperatively\n";
    return 1;
  }
  cancel.store(false, std::memory_order_release);
  result = pepepow::search_cuda_with_process(cpu, options, header, easy_target,
      5, 100, pepepow::SearchClock::now() + std::chrono::milliseconds(100));
  if (result.status != pepepow::SearchStatus::Timeout || result.candidate) {
    std::cerr << "parent deadline was not mapped to a bounded timeout\n";
    return 1;
  }
  if (temporary_entries() != before) {
    std::cerr << "cancelled search left a temporary process directory\n";
    return 1;
  }
  std::cout << "CUDA search process contract: candidate recheck, full digest and target "
               "rejection, cancellation, timeout, and temp cleanup passed\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string_view(argv[1]).starts_with("--mock-child-")) {
    const auto mode = std::string_view(argv[1]);
    return mock_child(argc, argv, mode == "--mock-child-cancel",
                      mode == "--mock-child-corrupt");
  }
  return run_contract_tests(argv[0]);
}
