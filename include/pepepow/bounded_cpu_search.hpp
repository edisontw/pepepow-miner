#pragma once
#include "pepepow/header_nonce.h"
#include "pepepow/uint256_target.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>

extern "C" void hoohashv110(const void* data, std::size_t len, std::uint8_t output[32]);

namespace pepepow {
using Header = std::array<std::uint8_t, PEPEPOW_HEADER_SIZE>;
using Digest = std::array<std::uint8_t, 32>;

struct CpuCandidate {
  Header header;
  Digest digest;
  std::uint32_t nonce;
};

enum class SearchStatus { Found, Exhausted, Timeout, Cancelled, Invalid };

struct SearchResult {
  SearchStatus status;
  std::optional<CpuCandidate> candidate;
  std::uint32_t hashes = 0;
};

inline bool valid_search_range(std::uint32_t start, std::uint32_t count) {
  return count > 0 && count <= 10000 &&
         count - 1 <= std::numeric_limits<std::uint32_t>::max() - start;
}

inline constexpr std::uint32_t default_search_timeout_ms = 30000;
inline constexpr std::uint32_t max_search_timeout_ms = 60000;

using SearchClock = std::chrono::steady_clock;

template <typename Stop>
SearchResult search_cpu_until(Header header, const Digest& target_le,
                              std::uint32_t start, std::uint32_t count,
                              SearchClock::time_point deadline, Stop stop) {
  if (!valid_search_range(start, count)) return {SearchStatus::Invalid, std::nullopt};
  for (std::uint32_t offset = 0; offset < count; ++offset) {
    if (stop()) return {SearchStatus::Cancelled, std::nullopt, offset};
    // A synchronous Core call cannot be interrupted mid-computation.
    if (SearchClock::now() >= deadline) return {SearchStatus::Timeout, std::nullopt, offset};
    const auto nonce = start + offset;
    pepepow_header_set_nonce(header.data(), header.size(), nonce);
    Digest digest{};
    hoohashv110(header.data(), header.size(), digest.data());
    if (stop()) return {SearchStatus::Cancelled, std::nullopt, offset + 1};
    if (SearchClock::now() >= deadline) return {SearchStatus::Timeout, std::nullopt, offset + 1};
    if (pepepow_uint256_meets_target_le(digest.data(), target_le.data()))
      return {SearchStatus::Found, CpuCandidate{header, digest, nonce}, offset + 1};
  }
  return {SearchStatus::Exhausted, std::nullopt, count};
}

inline SearchResult search_cpu(Header header, const Digest& target_le,
                               std::uint32_t start, std::uint32_t count,
                               std::uint32_t timeout_ms = default_search_timeout_ms,
                               const std::atomic_bool* cancel = nullptr) {
  if (timeout_ms == 0 || timeout_ms > max_search_timeout_ms)
    return {SearchStatus::Invalid, std::nullopt};
  return search_cpu_until(header, target_le, start, count,
                          SearchClock::now() + std::chrono::milliseconds(timeout_ms),
                          [cancel] { return cancel && cancel->load(std::memory_order_acquire); });
}
} // namespace pepepow
