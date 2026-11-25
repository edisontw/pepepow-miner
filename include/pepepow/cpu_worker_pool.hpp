#pragma once
#include "pepepow/bounded_cpu_search.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <new>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>

namespace pepepow {
inline constexpr unsigned max_cpu_workers = 256;

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

// The mining coordinator owns the pool and issues at most one search at a time.
// Workers reuse their threads across jobs and own their Core scratch per call.
class CpuWorkerPool {
 public:
  explicit CpuWorkerPool(unsigned workers) : workers_(workers) {
    if (workers < 2 || workers > max_cpu_workers) return;
    try {
      threads_.reserve(workers);
      for (unsigned index = 0; index < workers; ++index)
        threads_.emplace_back([this, index](std::stop_token stop) { worker(index, stop); });
    } catch (const std::system_error&) {
      threads_.clear();
    } catch (const std::bad_alloc&) {
      threads_.clear();
    }
  }

  SearchResult search(const Header& header, const Digest& target,
                      std::uint32_t start, std::uint32_t count,
                      SearchClock::time_point deadline, const std::atomic_bool* cancel = nullptr) {
    if (!valid_search_range(start, count) || workers_ == 0 || workers_ > max_cpu_workers)
      return {SearchStatus::Invalid, std::nullopt};
    const auto cancelled = [cancel] { return cancel && cancel->load(std::memory_order_acquire); };
    const unsigned active = std::min(workers_, count);
    if (active == 1) return search_cpu_until(header, target, start, count, deadline, cancelled);
    if (threads_.size() != workers_) return {SearchStatus::Invalid, std::nullopt};

    std::unique_lock lock(mutex_);
    task_ = {header, target, start, count, active, deadline, cancel};
    remaining_ = active;
    lowest_found_.store(active, std::memory_order_relaxed);
    ++epoch_;
    work_ready_.notify_all();
    work_done_.wait(lock, [this] { return remaining_ == 0; });

    std::uint32_t hashes = 0;
    for (unsigned index = 0; index < active; ++index) hashes += results_[index].hashes;
    if (cancelled()) return {SearchStatus::Cancelled, std::nullopt, hashes};
    // A timed-out lower partition may contain a match. Return the candidate
    // only if every partition preceding it was exhausted.
    for (unsigned index = 0; index < active; ++index) {
      if (results_[index].status != SearchStatus::Exhausted) {
        results_[index].hashes = hashes;
        return results_[index];
      }
    }
    return {SearchStatus::Exhausted, std::nullopt, hashes};
  }

 private:
  struct Task {
    Header header;
    Digest target;
    std::uint32_t start, count;
    unsigned active;
    SearchClock::time_point deadline;
    const std::atomic_bool* cancel;
  };

  void worker(unsigned index, std::stop_token stop) {
    std::uint64_t previous = 0;
    std::unique_lock lock(mutex_);
    while (work_ready_.wait(lock, stop, [&] { return epoch_ != previous; })) {
      previous = epoch_;
      if (index >= task_.active) continue;
      const Task task = task_;
      lock.unlock();
      const auto range = partition_nonce_range(task.start, task.count, index, task.active);
      auto result = search_cpu_until(task.header, task.target, range.start, range.count, task.deadline,
                                     [&] {
                                       return stop.stop_requested() ||
                                           index > lowest_found_.load(std::memory_order_relaxed) ||
                                           (task.cancel && task.cancel->load(std::memory_order_acquire));
                                     });
      lock.lock();
      // Higher partitions cannot improve a lower candidate. Lower partitions
      // finish so the coordinator can still select the lowest match.
      if (result.status == SearchStatus::Found &&
          index < lowest_found_.load(std::memory_order_relaxed))
        lowest_found_.store(index, std::memory_order_relaxed);
      results_[index] = std::move(result);
      if (--remaining_ == 0) work_done_.notify_one();
    }
  }

  const unsigned workers_;
  std::mutex mutex_;
  std::condition_variable_any work_ready_;
  std::condition_variable work_done_;
  Task task_{};
  std::array<SearchResult, max_cpu_workers> results_{};
  std::uint64_t epoch_ = 0;
  unsigned remaining_ = 0;
  std::atomic<unsigned> lowest_found_{max_cpu_workers};
  // Stop and join before destroying the state the threads access.
  std::vector<std::jthread> threads_;
};
} // namespace pepepow
