#include "pepepow/bounded_cpu_search.hpp"
#include "pepepow/job_state.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <string_view>

namespace {
template <std::size_t Size>
bool parse_hex(std::string_view text, std::array<std::uint8_t, Size>& bytes) {
  if (text.size() != Size * 2) return false;
  for (std::size_t i = 0; i < Size; ++i) {
    const auto* first = text.data() + i * 2;
    const auto [end, error] = std::from_chars(first, first + 2, bytes[i], 16);
    if (error != std::errc{} || end != first + 2) return false;
  }
  return true;
}

bool parse_decimal(std::string_view text, std::uint32_t& value) {
  if (text.empty()) return false;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

void usage() {
  std::puts("Usage: pepepow-offline-search HEADER_HEX TARGET_BE_HEX START COUNT [--replace-job] [--max-ms N]");
  std::puts("Offline only. COUNT=1..10000; --max-ms=1..60000 (default 30000); exit 4=timeout.");
}

void print_hex(const std::uint8_t* data, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) std::printf("%02x", static_cast<unsigned>(data[i]));
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    usage();
    return 0;
  }
  std::array<std::uint8_t, PEPEPOW_HEADER_SIZE> header{};
  std::array<std::uint8_t, 32> target{};
  std::uint32_t start = 0, count = 0;
  if (argc < 5 || !parse_hex(argv[1], header) || !parse_hex(argv[2], target) ||
      !parse_decimal(argv[3], start) || !parse_decimal(argv[4], count) ||
      !pepepow::valid_search_range(start, count)) {
    usage();
    return 2;
  }

  bool replace_job = false, timeout_given = false;
  std::uint32_t timeout_ms = pepepow::default_search_timeout_ms;
  for (int i = 5; i < argc; ++i) {
    const auto option = std::string_view(argv[i]);
    if (option == "--replace-job" && !replace_job) {
      replace_job = true;
    } else if (option == "--max-ms" && !timeout_given && i + 1 < argc &&
               parse_decimal(argv[i + 1], timeout_ms) &&
               timeout_ms > 0 && timeout_ms <= pepepow::max_search_timeout_ms) {
      timeout_given = true;
      ++i;
    } else {
      usage();
      return 2;
    }
  }

  std::reverse(target.begin(), target.end());
  pepepow::JobState jobs;
  jobs.connected();
  jobs.authorized(true);
  jobs.notify("fixture", true);
  const auto job = jobs.active().value();
  const auto result = pepepow::search_cpu(header, target, start, count, timeout_ms);
  if (result.status == pepepow::SearchStatus::Timeout) {
    std::puts("status=timeout");
    return 4;
  }
  if (!result.candidate) {
    std::puts("status=exhausted");
    return 1;
  }
  if (replace_job) jobs.notify(job.id, false);
  if (!jobs.can_submit(job)) {
    std::puts("status=stale");
    return 3;
  }
  const auto& candidate = *result.candidate;
  std::printf("status=found nonce=%llu digest_raw=", static_cast<unsigned long long>(candidate.nonce));
  print_hex(candidate.digest.data(), candidate.digest.size());
  std::printf("\nheader_hex=");
  print_hex(candidate.header.data(), candidate.header.size());
  std::putchar('\n');
  return 0;
}
