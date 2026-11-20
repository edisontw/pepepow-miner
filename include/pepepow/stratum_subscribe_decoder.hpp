#pragma once
#include "pepepow/job_state.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace pepepow {
struct SubscribeReply {
  std::string extranonce1;
  unsigned extranonce2_size;
};

// Strict fixed-order pool subscribe reply; not a general JSON-RPC parser.
inline std::optional<SubscribeReply> decode_subscribe_reply(
    std::string_view line, std::string_view expected_id) {
  if (line.size() > 65536 || expected_id.empty() ||
      (expected_id.size() > 1 && expected_id.front() == '0')) return std::nullopt;
  for (char c : expected_id) if (c < '0' || c > '9') return std::nullopt;
  std::size_t i = 0;
  auto ws = [&] { while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i; };
  auto take = [&](char c) { ws(); if (i == line.size() || line[i] != c) return false; ++i; return true; };
  auto lit = [&](std::string_view s) { ws(); if (line.substr(i,s.size()) != s) return false; i += s.size(); return true; };
  auto quoted = [&]() -> std::optional<std::string> {
    if (!take('"')) return std::nullopt;
    std::size_t start=i;
    while (i < line.size() && line[i]!='"') {
      unsigned char c=static_cast<unsigned char>(line[i]);
      if (c<32 || c>=127 || c=='\\') return std::nullopt;
      ++i;
    }
    if (i==line.size()) return std::nullopt;
    std::string value(line.substr(start,i-start)); ++i; return value;
  };
  if (!take('{') || !lit("\"id\"") || !take(':') || !lit(expected_id) ||
      !take(',') || !lit("\"result\"") || !take(':') || !take('[') || !take('[')) return std::nullopt;
  for (int n=0;n<2;++n) {
    if (n && !take(',')) return std::nullopt;
    if (!take('[')) return std::nullopt;
    auto method=quoted();
    if (!method || *method != (n==0 ? "mining.set_difficulty" : "mining.notify") || !take(',')) return std::nullopt;
    auto session=quoted();
    if (!session || session->empty() || !take(']')) return std::nullopt;
  }
  if (!take(']') || !take(',')) return std::nullopt;
  auto extranonce=quoted();
  if (!extranonce || extranonce->empty() || extranonce->size()%2) return std::nullopt;
  for (char c:*extranonce) if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return std::nullopt;
  if (!take(',')) return std::nullopt;
  ws(); unsigned size=0; std::size_t start=i;
  while (i<line.size() && line[i]>='0' && line[i]<='9') {
    size=size*10+static_cast<unsigned>(line[i]-'0');
    if (size>32) return std::nullopt;
    ++i;
  }
  if (start==i || (i-start>1 && line[start]=='0') || size==0 ||
      !take(']') || !take(',') || !lit("\"error\"") || !take(':') ||
      !lit("null") || !take('}')) return std::nullopt;
  ws(); if (i!=line.size()) return std::nullopt;
  return SubscribeReply{std::move(*extranonce),size};
}

// A subscription response is session state, not an active mining job.
// A changed extranonce invalidates queued work before any new notify.
class SubscribeWorkGuard {
 public:
  bool accept(const SubscribeReply& reply, JobState& jobs) {
    if (reply.extranonce1.empty() || reply.extranonce1.size() % 2 != 0 ||
        reply.extranonce2_size == 0 || reply.extranonce2_size > 32) return false;
    for (char c : reply.extranonce1) {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) return false;
    }
    if (current_ && (current_->extranonce1 != reply.extranonce1 ||
                     current_->extranonce2_size != reply.extranonce2_size)) {
      // Do not silently accept a reset when the session is not authorized.
      if (!jobs.apply(JobState::Event{JobState::EventKind::ExtranonceChanged}))
        return false;
    }
    current_ = reply;
    return true;
  }
  void reset() { current_.reset(); }
  const std::optional<SubscribeReply>& current() const { return current_; }
 private:
  std::optional<SubscribeReply> current_;
};
} // namespace pepepow
