#pragma once
#include <optional>
#include <string_view>
namespace pepepow {
// Validates one JSON value with bounded nesting, without allocating.
class JsonValueValidator {
 public:
  explicit JsonValueValidator(std::string_view input) : s_(input) {}
  bool parse(std::size_t& pos) { i_ = pos; if (!value(0)) return false; pos = i_; return true; }
 private:
  std::string_view s_;
  std::size_t i_ = 0;
  void ws() { while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) ++i_; }
  bool consume(char c) { ws(); if (i_ == s_.size() || s_[i_] != c) return false; ++i_; return true; }
  bool literal(std::string_view v) { if (s_.substr(i_, v.size()) != v) return false; i_ += v.size(); return true; }
  bool string() {
    if (!consume('"')) return false;
    while (i_ < s_.size()) {
      unsigned char c = static_cast<unsigned char>(s_[i_++]);
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c >= 0x80) {
        unsigned remaining = 0;
        unsigned char low = 0x80, high = 0xbf;
        if (c >= 0xc2 && c <= 0xdf) remaining = 1;
        else if (c >= 0xe0 && c <= 0xef) {
          remaining = 2;
          if (c == 0xe0) low = 0xa0;
          if (c == 0xed) high = 0x9f;
        } else if (c >= 0xf0 && c <= 0xf4) {
          remaining = 3;
          if (c == 0xf0) low = 0x90;
          if (c == 0xf4) high = 0x8f;
        } else return false;
        if (i_ == s_.size()) return false;
        auto first = static_cast<unsigned char>(s_[i_++]);
        if (first < low || first > high) return false;
        for (unsigned n = 1; n < remaining; ++n) {
          if (i_ == s_.size()) return false;
          auto next = static_cast<unsigned char>(s_[i_++]);
          if (next < 0x80 || next > 0xbf) return false;
        }
        continue;
      }
      if (c != '\\') continue;
      if (i_ == s_.size()) return false;
      char e = s_[i_++];
      if (e == 'u') {
        for (int j = 0; j < 4; ++j) {
          if (i_ == s_.size()) return false;
          char x = s_[i_++];
          if (!((x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F'))) return false;
        }
      } else if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f' && e != 'n' && e != 'r' && e != 't') return false;
    }
    return false;
  }
  bool number() {
    if (i_ < s_.size() && s_[i_] == '-') ++i_;
    if (i_ == s_.size()) return false;
    if (s_[i_] == '0') ++i_;
    else {
      if (s_[i_] < '1' || s_[i_] > '9') return false;
      while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
    }
    if (i_ < s_.size() && s_[i_] == '.') {
      ++i_; std::size_t start = i_;
      while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
      if (i_ == start) return false;
    }
    if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
      ++i_; if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
      std::size_t start = i_;
      while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
      if (i_ == start) return false;
    }
    return true;
  }
  bool value(unsigned depth) {
    ws(); if (i_ == s_.size()) return false;
    char c = s_[i_];
    if (c == '"') return string();
    if (c == 't') return literal("true");
    if (c == 'f') return literal("false");
    if (c == 'n') return literal("null");
    if (c != '[' && c != '{') return number();
    if (depth >= 32) return false;
    ++i_; const bool object = c == '{'; char end = object ? '}' : ']';
    ws(); if (i_ < s_.size() && s_[i_] == end) { ++i_; return true; }
    while (true) {
      if (object && (!string() || !consume(':'))) return false;
      if (!value(depth + 1)) return false;
      ws(); if (i_ < s_.size() && s_[i_] == end) { ++i_; return true; }
      if (!consume(',')) return false;
    }
  }
};

enum class SubmitOutcome { Accepted, Rejected, PoolError };
// Strict fixed-dialect JSON-RPC response. Never treats malformed input as accepted.
inline std::optional<SubmitOutcome> decode_submit_reply(std::string_view line,
                                                         std::string_view expected_id) {
  if (line.size() > 65536 || expected_id.empty() ||
      (expected_id.size() > 1 && expected_id.front() == '0')) return std::nullopt;
  for (char c : expected_id) if (c < '0' || c > '9') return std::nullopt;
  std::size_t i = 0;
  auto ws = [&] { while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i; };
  auto take = [&](char c) { ws(); if (i >= line.size() || line[i] != c) return false; ++i; return true; };
  auto lit = [&](std::string_view s) { ws(); if (line.substr(i, s.size()) != s) return false; i += s.size(); return true; };
  if (!take('{') || !lit("\"id\"") || !take(':') || !lit(expected_id) ||
      !take(',') || !lit("\"result\"") || !take(':')) return std::nullopt;
  std::optional<bool> accepted;
  if (lit("true")) accepted = true;
  else if (lit("false")) accepted = false;
  else if (!lit("null")) return std::nullopt;
  if (!take(',') || !lit("\"error\"") || !take(':')) return std::nullopt;
  if (lit("null")) {
    if (!take('}')) return std::nullopt;
    ws();
    if (i != line.size() || !accepted) return std::nullopt;
    return *accepted ? SubmitOutcome::Accepted : SubmitOutcome::Rejected;
  }
  ws();
  if (i >= line.size() || (line[i] != '[' && line[i] != '{')) return std::nullopt;
  JsonValueValidator validator(line);
  if (!validator.parse(i) || !take('}')) return std::nullopt;
  ws();
  if (i != line.size() || accepted == true) return std::nullopt;
  return SubmitOutcome::PoolError;
}
} // namespace pepepow
