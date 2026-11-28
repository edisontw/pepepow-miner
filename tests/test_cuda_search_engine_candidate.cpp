#include "pepepow/bounded_cpu_search.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

extern "C" void hoohashv110(const void*, std::size_t, std::uint8_t[32]);

namespace {

std::string_view argument_value(int argc, char** argv, std::string_view name) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) == name) return argv[index + 1];
  }
  return {};
}

template <typename Integer>
bool parse_integer(std::string_view text, Integer& value) {
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

int hex_nibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool decode_hex(std::string_view text, std::uint8_t* output, std::size_t size) {
  if (text.size() != size * 2) return false;
  for (std::size_t index = 0; index < size; ++index) {
    const int high = hex_nibble(text[index * 2]);
    const int low = hex_nibble(text[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    output[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

template <typename Bytes>
void write_hex(std::ostream& output, const Bytes& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  for (const auto byte : bytes) {
    output.put(digits[byte >> 4]);
    output.put(digits[byte & 15]);
  }
}

#ifdef PEPEPOW_TEST_CUDA_STALE_CANDIDATE
std::string ready_file_path() {
#ifdef _MSC_VER
  char* value = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&value, &length, "PEPEPOW_TEST_CUDA_READY_FILE") != 0 ||
      !value || length <= 1) {
    std::free(value);
    return {};
  }
  std::string result(value);
  std::free(value);
  return result;
#else
  const char* value = std::getenv("PEPEPOW_TEST_CUDA_READY_FILE");
  return value ? std::string(value) : std::string{};
#endif
}

bool append_marker(const std::string& path, std::string_view marker) {
  std::ofstream output(path, std::ios::app);
  if (!output) return false;
  output << marker << '\n';
  output.flush();
  return output.good();
}
#endif

int find_and_emit_candidate(const pepepow::Header& header,
                            const pepepow::Digest& target,
                            std::uint32_t nonce_start,
                            std::uint32_t nonce_count,
                            std::uint32_t batch_size) {
  // Skip nonce_start to exercise a nonzero header and Stratum nonce encoding.
  const std::uint32_t first_offset = nonce_count > 1 ? 1 : 0;
  for (std::uint32_t offset = first_offset; offset < nonce_count; ++offset) {
    const auto nonce = static_cast<std::uint32_t>(nonce_start + offset);
    auto candidate_header = header;
    pepepow_header_set_nonce(candidate_header.data(), candidate_header.size(), nonce);
    pepepow::Digest digest{};
    hoohashv110(candidate_header.data(), candidate_header.size(), digest.data());
    if (!pepepow_uint256_meets_target_le(digest.data(), target.data())) continue;

    const auto processed = offset + 1;
    const auto batch_count =
        (static_cast<std::uint64_t>(processed) + batch_size - 1) / batch_size;
    std::cout << "{\"schema_version\":1,\"status\":\"ok\","
                 "\"engine\":\"cuda_hoohash_v110_bounded_search\","
                 "\"search_status\":\"found\",\"stop_reason\":null,"
                 "\"nonce_start\":"
              << nonce_start << ",\"nonce_count\":" << nonce_count
              << ",\"processed_count\":" << processed
              << ",\"next_nonce\":"
              << static_cast<std::uint64_t>(nonce_start) + processed
              << ",\"batch_size\":" << batch_size
              << ",\"batch_count\":" << batch_count
              << ",\"target_encoding\":\"little_endian_uint256\","
                 "\"cpu_recheck\":\"pinned_core_full_digest_and_target\","
                 "\"candidates\":[{\"nonce\":" << nonce << ",\"digest_hex\":\"";
    write_hex(std::cout, digest);
    std::cout << "\"}],\"error\":null}\n";
    return std::cout ? 0 : 65;
  }

  std::cerr << "bounded candidate fixture found no nonce meeting target\n";
  return 66;
}

}  // namespace

int main(int argc, char** argv) {
  pepepow::Header header{};
  pepepow::Digest target{};
  std::uint32_t nonce_start = 0;
  std::uint32_t nonce_count = 0;
  std::uint32_t batch_size = 0;
  std::uint32_t timeout_ms = 0;
  if (!decode_hex(argument_value(argc, argv, "--header-hex"),
                  header.data(), header.size()) ||
      !decode_hex(argument_value(argc, argv, "--target-hex"),
                  target.data(), target.size()) ||
      !parse_integer(argument_value(argc, argv, "--nonce-start"), nonce_start) ||
      !parse_integer(argument_value(argc, argv, "--nonce-count"), nonce_count) ||
      !parse_integer(argument_value(argc, argv, "--batch-size"), batch_size) ||
      !parse_integer(argument_value(argc, argv, "--timeout-ms"), timeout_ms) ||
      nonce_count == 0 || nonce_count > 10000000 || batch_size == 0 ||
      batch_size > 256 || timeout_ms == 0 ||
      static_cast<std::uint64_t>(nonce_count) >
          (std::uint64_t{1} << 32) - nonce_start) {
    std::cerr << "invalid bounded candidate fixture request\n";
    return 64;
  }

#ifdef PEPEPOW_TEST_CUDA_STALE_CANDIDATE
  const auto ready_path = ready_file_path();
  if (ready_path.empty() || !append_marker(ready_path, "ready")) return 67;
  const auto cancel_path = std::filesystem::path(
      std::string(argument_value(argc, argv, "--cancel-file")));
  const auto stop_at = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeout_ms + 2000U);
  std::error_code error;
  while (!std::filesystem::exists(cancel_path, error) &&
         std::chrono::steady_clock::now() < stop_at) {
    error.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if (error || !std::filesystem::exists(cancel_path, error) ||
      !append_marker(ready_path, "candidate_after_cancel")) {
    std::cerr << "candidate fixture did not observe the cancellation request\n";
    return 68;
  }
#endif

  return find_and_emit_candidate(header, target, nonce_start, nonce_count, batch_size);
}
