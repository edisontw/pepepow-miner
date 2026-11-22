#pragma once
#include "pepepow/job_state.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pepepow {
struct NotifyWork {
  std::string prevhash, coinb1, coinb2, version, bits, ntime;
  std::vector<std::string> merkle_branch;
};

// Fixed-order PEPEPOW pool dialect. Publish work fields only after validation.
inline std::optional<JobState::Event> decode_notify(std::string_view line,
                                                   NotifyWork* work = nullptr) {
  if (line.size() > 65536) return std::nullopt;
  std::size_t i = 0;
  auto ws = [&] { while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i; };
  auto take = [&](char c) { ws(); if (i == line.size() || line[i] != c) return false; ++i; return true; };
  auto literal = [&](std::string_view s) { ws(); if (line.substr(i,s.size()) != s) return false; i += s.size(); return true; };
  auto str = [&]() -> std::optional<std::string> {
    if (!take('"')) return std::nullopt;
    const auto start = i;
    while (i < line.size() && line[i] != '"') {
      const unsigned char c = static_cast<unsigned char>(line[i]);
      if (c < 0x20 || c == '\\' || c >= 0x7f) return std::nullopt;
      ++i;
    }
    if (i == line.size()) return std::nullopt;
    auto value = std::string(line.substr(start, i-start)); ++i;
    return value;
  };
  auto hex = [&](std::string& field, std::size_t exact, bool allow_empty=false) {
    auto s = str();
    if (!s || (exact && s->size()!=exact) || (!exact && !allow_empty && s->empty()) || s->size()%2) return false;
    for (char c : *s) if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return false;
    field = std::move(*s);
    return true;
  };
  NotifyWork decoded;
  if (!take('{') || !literal("\"id\"") || !take(':') || !literal("null") ||
      !take(',') || !literal("\"method\"") || !take(':') ||
      !literal("\"mining.notify\"") || !take(',') ||
      !literal("\"params\"") || !take(':') || !take('[')) return std::nullopt;
  auto job = str();
  if (!job || job->empty() || !take(',') || !hex(decoded.prevhash, 64) || !take(',') ||
      !hex(decoded.coinb1, 0, true) || !take(',') || !hex(decoded.coinb2, 0, true) || !take(',') || !take('[')) return std::nullopt;
  ws();
  if (i < line.size() && line[i] != ']') {
    for (;;) {
      std::string sibling;
      if (!hex(sibling, 64)) return std::nullopt;
      if (work) decoded.merkle_branch.push_back(std::move(sibling));
      ws(); if (i < line.size() && line[i] == ']') break;
      if (!take(',')) return std::nullopt;
    }
  }
  if (!take(']') || !take(',') || !hex(decoded.version, 8) || !take(',') || !hex(decoded.bits, 8) ||
      !take(',') || !hex(decoded.ntime, 8) || !take(',')) return std::nullopt;
  bool clean = false;
  if (literal("true")) clean = true;
  else if (literal("false")) clean = false;
  else if (literal("1")) clean = true;
  else if (!literal("0")) return std::nullopt;
  if (!take(']') || !take('}')) return std::nullopt;
  ws(); if (i != line.size()) return std::nullopt;
  if (work) *work = std::move(decoded);
  return JobState::Event{JobState::EventKind::Notify, false, 0.0, std::move(*job), clean};
}
} // namespace pepepow
