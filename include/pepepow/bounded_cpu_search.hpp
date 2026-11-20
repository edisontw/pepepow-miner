#pragma once
#include "pepepow/header_nonce.h"
#include "pepepow/uint256_target.h"

#include <array>
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

enum class SearchStatus { Found, Exhausted, Timeout, Invalid };

struct SearchResult {
  SearchStatus status;
  std::optional<CpuCandidate> candidate;
};

inline bool valid_search_range(std::uint32_t start, std::uint32_t count) {
  return count > 0 && count <= 10000 &&
         count - 1 <= std::numeric_limits<std::uint32_t>::max() - start;
}

inline constexpr std::uint32_t default_search_timeout_ms = 30000;
inline constexpr std::uint32_t max_search_timeout_ms = 60000;

inline SearchResult search_cpu(Header header, const Digest& target_le,
                               std::uint32_t start, std::uint32_t count,
                               std::uint32_t timeout_ms = default_search_timeout_ms) {
  if (!valid_search_range(start, count) || timeout_ms == 0 ||
      timeout_ms > max_search_timeout_ms) return {SearchStatus::Invalid, std::nullopt};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  for (std::uint32_t offset = 0; offset < count; ++offset) {
    // The synchronous Core call cannot be interrupted by this cooperative deadline.
    if (std::chrono::steady_clock::now() >= deadline)
      return {SearchStatus::Timeout, std::nullopt};
    const auto nonce = start + offset;
    pepepow_header_set_nonce(header.data(), header.size(), nonce);
    Digest digest{};
    hoohashv110(header.data(), header.size(), digest.data());
    if (std::chrono::steady_clock::now() >= deadline)
      return {SearchStatus::Timeout, std::nullopt};
    if (pepepow_uint256_meets_target_le(digest.data(), target_le.data()))
      return {SearchStatus::Found, CpuCandidate{header, digest, nonce}};
  }
  return {SearchStatus::Exhausted, std::nullopt};
}
} // namespace pepepow
