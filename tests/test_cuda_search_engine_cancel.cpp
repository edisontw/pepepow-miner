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

namespace {

std::string_view argument_value(int argc, char** argv, std::string_view name) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string_view(argv[index]) == name) return argv[index + 1];
  }
  return {};
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

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

}  // namespace

int main(int argc, char** argv) {
  std::uint32_t nonce_start = 0;
  std::uint32_t nonce_count = 0;
  std::uint32_t batch_size = 0;
  std::uint32_t timeout_ms = 0;
  const auto cancel_path = argument_value(argc, argv, "--cancel-file");
  if (cancel_path.empty() ||
      !parse_u32(argument_value(argc, argv, "--nonce-start"), nonce_start) ||
      !parse_u32(argument_value(argc, argv, "--nonce-count"), nonce_count) ||
      !parse_u32(argument_value(argc, argv, "--batch-size"), batch_size) ||
      !parse_u32(argument_value(argc, argv, "--timeout-ms"), timeout_ms) ||
      nonce_count == 0 || batch_size == 0 || timeout_ms == 0)
    return 64;

  const auto ready_path = ready_file_path();
  if (ready_path.empty()) return 65;
  if (!append_marker(ready_path, "ready")) return 66;

  const auto cancel_file = std::filesystem::path(std::string(cancel_path));
  const auto wait_deadline = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeout_ms + 3000U);
  std::error_code error;
  while (!std::filesystem::exists(cancel_file, error) &&
         std::chrono::steady_clock::now() < wait_deadline) {
    error.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!std::filesystem::exists(cancel_file, error) || error) return 68;
  if (!append_marker(ready_path, "cancelled")) return 69;

  std::cout << "{\"schema_version\":1,\"status\":\"cancelled\","
            "\"engine\":\"cuda_hoohash_v110_bounded_search\","
            "\"search_status\":\"cancelled\",\"stop_reason\":\"cancel_file\","
            "\"nonce_start\":" << nonce_start
         << ",\"nonce_count\":" << nonce_count
         << ",\"processed_count\":0,\"next_nonce\":" << nonce_start
         << ",\"batch_size\":" << batch_size
         << ",\"batch_count\":0,\"target_encoding\":\"little_endian_uint256\","
            "\"cpu_recheck\":\"pinned_core_full_digest_and_target\","
            "\"candidates\":[],\"error\":null}\n";
  std::cout.flush();
  return std::cout ? 130 : 70;
}
