#pragma once

#include <cstdint>

namespace pepepow {

inline constexpr std::uint32_t cpu_benchmark_batch_size = 10000;

struct CpuBenchmarkBatchAccounting {
  std::uint64_t complete_batches;
  std::uint32_t final_batch_hashes;
};

constexpr CpuBenchmarkBatchAccounting account_cpu_benchmark_hashes(
    std::uint64_t total_hashes) {
  return {total_hashes / cpu_benchmark_batch_size,
          static_cast<std::uint32_t>(total_hashes % cpu_benchmark_batch_size)};
}

}  // namespace pepepow
