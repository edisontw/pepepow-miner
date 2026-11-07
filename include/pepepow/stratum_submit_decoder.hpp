#pragma once
#include <optional>
#include <string_view>
namespace pepepow {
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
  char opener = line[i++];
  int depth = 1;
  bool in_string = false, escape = false;
  while (i < line.size() && depth) {
    char c = line[i++];
    if (in_string) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '"') in_string = false;
    } else if (c == '"') in_string = true;
    else if (c == opener) ++depth;
    else if (c == (opener == '[' ? ']' : '}')) --depth;
  }
  if (depth || in_string || !take('}')) return std::nullopt;
  ws();
  if (i != line.size() || accepted == true) return std::nullopt;
  return SubmitOutcome::PoolError;
}
} // namespace pepepow
