#include "cpu_benchmark_accounting.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

int main() {
  using pepepow::account_cpu_benchmark_hashes;

  const auto empty = account_cpu_benchmark_hashes(0);
  if (empty.complete_batches != 0 || empty.final_batch_hashes != 0) {
    std::cerr << "empty benchmark accounting was not zero\n";
    return 1;
  }

  const auto partial = account_cpu_benchmark_hashes(9999);
  if (partial.complete_batches != 0 || partial.final_batch_hashes != 9999) {
    std::cerr << "partial batch accounting was incorrect\n";
    return 1;
  }

  const auto exact = account_cpu_benchmark_hashes(20000);
  if (exact.complete_batches != 2 || exact.final_batch_hashes != 0) {
    std::cerr << "full batch accounting was incorrect\n";
    return 1;
  }

  constexpr std::uint64_t nonce_space =
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1;
  constexpr auto exhausted = account_cpu_benchmark_hashes(nonce_space);
  static_assert(exhausted.complete_batches == 429496);
  static_assert(exhausted.final_batch_hashes == 7296);
  if (exhausted.complete_batches * pepepow::cpu_benchmark_batch_size +
          exhausted.final_batch_hashes !=
      nonce_space) {
    std::cerr << "exhausted uint32 nonce-space accounting was incorrect\n";
    return 1;
  }

  std::cout << "PASS: full, partial, and exhausted nonce batch accounting\n";
  return 0;
}
