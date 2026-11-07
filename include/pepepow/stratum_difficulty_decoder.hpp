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
} // namespace pepepow
