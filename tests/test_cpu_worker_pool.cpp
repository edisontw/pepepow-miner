#include "pepepow/cpu_worker_pool.hpp"

#include <atomic>
#include <barrier>
#include <cfenv>
#include <iostream>
#include <source_location>
#include <thread>
#include <vector>

namespace {
bool failure(std::source_location where = std::source_location::current()) {
  std::cerr << where.file_name() << ':' << where.line() << '\n';
  return false;
}

bool verify_worker_count(unsigned workers) {
  using namespace pepepow;
  constexpr std::uint32_t first = UINT32_MAX - 31;
  constexpr std::uint32_t count = 32;
  Header header{};
  for (std::size_t index = 0; index < header.size(); ++index)
    header[index] = static_cast<std::uint8_t>(7 * index);

  std::array<Digest, count> expected{};
  for (std::uint32_t offset = 0; offset < count; ++offset) {
    pepepow_header_set_nonce(header.data(), header.size(), first + offset);
    hoohashv110(header.data(), header.size(), expected[offset].data());
  }

  std::atomic_bool mismatch = false;
  std::barrier start_line(static_cast<std::ptrdiff_t>(workers));
  std::vector<std::jthread> threads;
  std::uint64_t next = first;
  const int rounding = std::fegetround();
  for (unsigned index = 0; index < workers; ++index) {
    const auto range = partition_nonce_range(first, count, index, workers);
    if (range.start != next || range.count == 0) mismatch = true;
    next += range.count;
    threads.emplace_back([&, range, local = header]() mutable {
      start_line.arrive_and_wait();
      for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto nonce = range.start + offset;
        pepepow_header_set_nonce(local.data(), local.size(), nonce);
        Digest actual{};
        hoohashv110(local.data(), local.size(), actual.data());
        if (actual != expected[nonce - first] || std::fegetround() != rounding)
          mismatch.store(true, std::memory_order_relaxed);
      }
    });
  }
  for (auto& thread : threads) thread.join();
  if (mismatch || next != std::uint64_t{UINT32_MAX} + 1) return failure();

  CpuWorkerPool pool(workers);
  const auto deadline = SearchClock::now() + std::chrono::seconds(5);
  const auto rejected = pool.search(header, Digest{}, first, count, deadline);
  if (rejected.status != SearchStatus::Exhausted || rejected.hashes != count)
    return failure();

  const auto exact = pool.search(header, expected.back(), UINT32_MAX, 1, deadline);
  if (!exact.candidate || exact.candidate->nonce != UINT32_MAX ||
      exact.candidate->digest != expected.back() || exact.hashes != 1)
    return failure();
  if (pool.search(header, Digest{}, UINT32_MAX, 2, deadline).status != SearchStatus::Invalid)
    return failure();

  Digest easy;
  easy.fill(0xff);
  const auto found = pool.search(header, easy, first, count, deadline);
  if (!found.candidate || found.candidate->nonce != first ||
      found.candidate->digest != expected.front())
    return failure();
  return true;
}
}  // namespace

int main() {
  for (const unsigned workers : {1u, 2u, 4u, 8u, 12u}) {
    if (!verify_worker_count(workers)) return 1;
    std::cout << "PASS: " << workers << " workers, full 32-byte parity, disjoint nonce coverage, "
                 "uint32 boundary, target equality/rejection, and overflow rejection\n";
  }
  return 0;
}
