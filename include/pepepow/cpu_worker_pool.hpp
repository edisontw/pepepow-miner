#pragma once
#include "pepepow/bounded_cpu_search.hpp"

#include <algorithm>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>

namespace pepepow {
inline constexpr unsigned max_cpu_workers = 8;

struct NonceRange {
  std::uint32_t start;
  std::uint32_t count;
};

// Endpoints are offsets from the validated start. The exclusive end can
// represent UINT32_MAX + 1 without ever converting that value to a nonce.
inline NonceRange partition_nonce_range(std::uint32_t start, std::uint32_t count,
                                        unsigned index, unsigned workers) {
  const std::uint64_t first = std::uint64_t{count} * index / workers;
  const std::uint64_t end = std::uint64_t{count} * (index + 1) / workers;
  return {static_cast<std::uint32_t>(start + first), static_cast<std::uint32_t>(end - first)};
}

inline SearchResult search_cpu_workers(const Header& header, const Digest& target,
                                       std::uint32_t start, std::uint32_t count,
                                       unsigned workers, SearchClock::time_point deadline,
                                       const std::atomic_bool* cancel = nullptr) {
  if (!valid_search_range(start, count) || workers == 0 || workers > max_cpu_workers)
    return {SearchStatus::Invalid, std::nullopt};
  workers = std::min(workers, count);
  const auto cancelled = [cancel] { return cancel && cancel->load(std::memory_order_acquire); };
  if (workers == 1) return search_cpu_until(header, target, start, count, deadline, cancelled);

  std::array<SearchResult, max_cpu_workers> results{};
  std::vector<std::jthread> threads;
  threads.reserve(workers);
  try {
    for (unsigned index = 0; index < workers; ++index) {
      const auto range = partition_nonce_range(start, count, index, workers);
      threads.emplace_back([&, index, range](std::stop_token stop) {
        results[index] = search_cpu_until(header, target, range.start, range.count, deadline,
                                         [&] { return stop.stop_requested() || cancelled(); });
      });
    }
  } catch (const std::system_error&) {
    for (auto& thread : threads) thread.request_stop();
    for (auto& thread : threads) thread.join();
    return {SearchStatus::Invalid, std::nullopt};
  }
  for (auto& thread : threads) thread.join();

  std::uint32_t hashes = 0;
  for (unsigned index = 0; index < workers; ++index) hashes += results[index].hashes;
  if (cancelled()) return {SearchStatus::Cancelled, std::nullopt, hashes};
  // The first non-exhausted partition determines the lowest proven candidate.
  // A timed-out lower partition may contain a match, so report its timeout.
  for (unsigned index = 0; index < workers; ++index) {
    if (results[index].status != SearchStatus::Exhausted) {
      results[index].hashes = hashes;
      return results[index];
    }
  }
  return {SearchStatus::Exhausted, std::nullopt, hashes};
}
} // namespace pepepow
