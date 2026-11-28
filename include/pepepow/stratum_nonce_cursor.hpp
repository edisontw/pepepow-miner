#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace pepepow {

inline constexpr std::uint64_t stratum_nonce_space_size = std::uint64_t{1} << 32;

struct StratumNonceBatch {
  std::uint32_t start;
  std::uint32_t count;
};

class StratumNonceCursor {
 public:
  [[nodiscard]] std::uint64_t next_nonce() const noexcept { return next_nonce_; }
  [[nodiscard]] bool exhausted() const noexcept {
    return next_nonce_ == stratum_nonce_space_size;
  }

  void reset() noexcept { next_nonce_ = 0; }

  [[nodiscard]] std::optional<StratumNonceBatch> reserve(
      std::uint32_t maximum_count) const noexcept {
    if (maximum_count == 0 || exhausted()) return std::nullopt;
    const auto remaining = stratum_nonce_space_size - next_nonce_;
    const auto count = (std::min)(static_cast<std::uint64_t>(maximum_count), remaining);
    return StratumNonceBatch{static_cast<std::uint32_t>(next_nonce_),
                             static_cast<std::uint32_t>(count)};
  }

  [[nodiscard]] bool advance_exhausted_batch(std::uint32_t count) noexcept {
    if (count == 0 || exhausted() ||
        static_cast<std::uint64_t>(count) > stratum_nonce_space_size - next_nonce_)
      return false;
    next_nonce_ += count;
    return true;
  }

  [[nodiscard]] bool advance_after_candidate(std::uint32_t nonce) noexcept {
    const auto value = static_cast<std::uint64_t>(nonce);
    if (value < next_nonce_ || value >= stratum_nonce_space_size) return false;
    next_nonce_ = value + 1;
    return true;
  }

 private:
  std::uint64_t next_nonce_ = 0;
};

inline bool increment_stratum_extranonce2(std::string& value) {
  if (value.size() % 2 != 0) return false;
  constexpr char digits[] = "0123456789abcdef";
  std::size_t increment_index = value.size();

  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
          (ch >= 'A' && ch <= 'F')))
      return false;
    const auto nibble = ch >= '0' && ch <= '9' ? static_cast<unsigned>(ch - '0') :
        ch >= 'a' && ch <= 'f' ? static_cast<unsigned>(ch - 'a' + 10) :
                                  static_cast<unsigned>(ch - 'A' + 10);
    if (nibble != 15) increment_index = i;
  }

  if (increment_index == value.size()) return false;
  const char ch = value[increment_index];
  const auto nibble = ch >= '0' && ch <= '9' ? static_cast<unsigned>(ch - '0') :
      ch >= 'a' && ch <= 'f' ? static_cast<unsigned>(ch - 'a' + 10) :
                                static_cast<unsigned>(ch - 'A' + 10);
  value[increment_index] = digits[nibble + 1];
  for (std::size_t i = increment_index + 1; i < value.size(); ++i) value[i] = '0';
  return true;
}

}  // namespace pepepow
