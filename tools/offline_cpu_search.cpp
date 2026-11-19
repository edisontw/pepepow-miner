#include "pepepow/header_nonce.h"
#include "pepepow/job_state.hpp"
#include "pepepow/uint256_target.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <limits>
#include <string_view>

extern "C" void hoohashv110(const void* data, std::size_t len, std::uint8_t output[32]);

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
  std::puts("Usage: pepepow-offline-search HEADER_HEX TARGET_BE_HEX START COUNT [--replace-job]");
  std::puts("Offline fixture only. COUNT is 1..10000. --replace-job simulates stale work.");
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
  const bool replace_job = argc == 6 && std::string_view(argv[5]) == "--replace-job";
  if ((argc != 5 && !replace_job) || !parse_hex(argv[1], header) ||
      !parse_hex(argv[2], target) || !parse_decimal(argv[3], start) ||
      !parse_decimal(argv[4], count) || count == 0 || count > 10000 ||
      count - 1 > std::numeric_limits<std::uint32_t>::max() - start) {
    usage();
    return 2;
  }

  std::reverse(target.begin(), target.end());
  pepepow::JobState jobs;
  jobs.connected();
  jobs.authorized(true);
  jobs.notify("fixture", true);
  const auto job = jobs.active().value();
  for (std::uint32_t offset = 0; offset < count; ++offset) {
    const auto nonce = start + offset;
    pepepow_header_set_nonce(header.data(), header.size(), nonce);
    std::array<std::uint8_t, 32> digest{};
    hoohashv110(header.data(), header.size(), digest.data());
    if (!pepepow_uint256_meets_target_le(digest.data(), target.data())) {
      continue;
    }
    // Replace the job after finding a valid candidate to exercise the acceptance gate.
    if (replace_job) jobs.notify(job.id, false);
    if (!jobs.can_submit(job)) {
      std::puts("status=stale");
      return 3;
    }
    std::printf("status=found nonce=%llu digest_raw=", static_cast<unsigned long long>(nonce));
    for (const auto byte : digest) std::printf("%02x", static_cast<unsigned>(byte));
    std::putchar('\n');
    return 0;
  }
  std::puts("status=exhausted");
  return 1;
}
