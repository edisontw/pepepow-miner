#pragma once
#include "pepepow/job_state.hpp"
#include <charconv>
#include <optional>
#include <string_view>

namespace pepepow {
// Strict subset decoder: only the fixed-order pool difficulty notification.
// Unknown JSON-RPC shapes must be handled by a future general decoder.
inline std::optional<JobState::Event> decode_difficulty(std::string_view line) {
  if (line.size() > 65536) return std::nullopt;
  std::size_t i = 0;
  auto ws = [&] { while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i; };
  auto take = [&](char c) { ws(); if (i == line.size() || line[i] != c) return false; ++i; return true; };
  auto literal = [&](std::string_view s) {
    ws(); if (line.substr(i, s.size()) != s) return false;
    i += s.size(); return true;
  };
  if (!take('{') || !literal("\"id\"") || !take(':') || !literal("null") ||
      !take(',') || !literal("\"method\"") || !take(':') ||
      !literal("\"mining.set_difficulty\"") || !take(',') ||
      !literal("\"params\"") || !take(':') || !take('[')) return std::nullopt;
  ws();
  const std::size_t start = i;
  if (i < line.size() && line[i] == '-') ++i;
  if (i == line.size() || line[i] < '0' || line[i] > '9') return std::nullopt;
  if (line[i] == '0') ++i;
  else while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
  if (i < line.size() && line[i] == '.') {
    ++i; const auto digits = i;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    if (i == digits) return std::nullopt;
  }
  if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
    ++i;
    if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
    const auto digits = i;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    if (i == digits) return std::nullopt;
  }
  double difficulty = 0;
  auto [end, error] = std::from_chars(line.data() + start, line.data() + i, difficulty);
  if (error != std::errc{} || end != line.data() + i ||
      !(difficulty > 0.0) || difficulty > 1.7976931348623157e308 ||
      !take(']') || !take('}')) return std::nullopt;
  ws();
  if (i != line.size()) return std::nullopt;
  return JobState::Event{JobState::EventKind::Difficulty, false, difficulty};
}

 // Strict offline authorization response decoder. The caller supplies the
 // outstanding authorize request ID; unrelated replies cannot authorize work.
inline std::optional<JobState::Event> decode_authorize_reply(
    std::string_view line, std::string_view expected_id) {
  if (line.size() > 65536 || expected_id.empty()) return std::nullopt;
  std::size_t i = 0;
  auto ws = [&] { while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i; };
  auto take = [&](char c) {
    ws(); if (i == line.size() || line[i] != c) return false;
    ++i; return true;
  };
  auto literal = [&](std::string_view s) {
    ws(); if (line.substr(i, s.size()) != s) return false;
    i += s.size(); return true;
  };
  // IDs are positive decimal integers, not JSON strings or arbitrary tokens.
  if (!take('{') || !literal("\"id\"") || !take(':')) return std::nullopt;
  ws();
  const auto start = i;
  while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
  if (start == i || (i - start > 1 && line[start] == '0') ||
      line.substr(start, i - start) != expected_id ||
      !take(',') || !literal("\"result\"") || !take(':')) return std::nullopt;
  bool authorized = false;
  if (literal("true")) authorized = true;
  else if (!literal("false")) return std::nullopt;
  if (!take(',') || !literal("\"error\"") || !take(':') ||
      !literal("null") || !take('}')) return std::nullopt;
  ws();
  if (i != line.size()) return std::nullopt;
  return JobState::Event{JobState::EventKind::Authorized, authorized};
}
} // namespace pepepow
