#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace pepepow {
// Offline newline framing only; JSON decoding and network I/O are separate.
class StratumLineFramer {
 public:
  static constexpr std::size_t max_line = 65536;
  struct Result {
    std::vector<std::string> lines;
    std::size_t rejected = 0;
  };
  Result feed(std::string_view bytes) {
    Result result;
    for (char c : bytes) {
      if (c == '\n') {
        if (discarding_) {
          ++result.rejected;
        } else {
          if (!buffer_.empty() && buffer_.back() == '\r') buffer_.pop_back();
          if (!buffer_.empty()) result.lines.push_back(std::move(buffer_));
        }
        buffer_.clear();
        discarding_ = false;
      } else if (discarding_ || c == '\0' || (c == '\r' && !buffer_.empty() && buffer_.back() == '\r') || (c != '\r' && !buffer_.empty() && buffer_.back() == '\r') || buffer_.size() >= max_line + (c == '\r' ? 1u : 0u)) {
        discarding_ = true;
        buffer_.clear();
      } else {
        buffer_.push_back(c);
      }
    }
    return result;
  }
  void reset() { buffer_.clear(); discarding_ = false; }
 private:
  std::string buffer_;
  bool discarding_ = false;
};
}  // namespace pepepow
