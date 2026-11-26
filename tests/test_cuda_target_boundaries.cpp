#include "pepepow/uint256_target.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

bool increment(std::array<std::uint8_t, 32>& value) {
  for (auto& byte : value) {
    if (++byte != 0) return true;
  }
  return false;
}

bool decrement(std::array<std::uint8_t, 32>& value) {
  for (auto& byte : value) {
    if (byte-- != 0) return true;
  }
  return false;
}

}  // namespace

int main() {
  std::array<std::uint8_t, 32> target{};
  target[0] = 0x42;
  target[7] = 0x91;
  target[31] = 0x01;

  auto hash = target;
  if (!pepepow_uint256_meets_target_le(hash.data(), target.data())) return 1;

  hash = target;
  if (!decrement(hash) ||
      !pepepow_uint256_meets_target_le(hash.data(), target.data())) {
    return 1;
  }

  hash = target;
  if (!increment(hash) ||
      pepepow_uint256_meets_target_le(hash.data(), target.data())) {
    return 1;
  }

  std::cout << "CPU target boundary contract: equality, target-1, target+1 PASS\n";
  return 0;
}
