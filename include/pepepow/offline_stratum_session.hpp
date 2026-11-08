#pragma once
#include "pepepow/stratum_difficulty_decoder.hpp"
#include "pepepow/stratum_notify_decoder.hpp"
#include "pepepow/stratum_subscribe_decoder.hpp"
#include "pepepow/stratum_submit_decoder.hpp"
#include <cstdint>
#include <string>
#include <unordered_set>
#include <string_view>

namespace pepepow {
// Offline fixed-dialect dispatcher. No socket, hashing, or share submission.
// A session must subscribe and authorize before accepting mining work.
class OfflineStratumSession {
 public:
  struct ShareCounters {
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t pool_errors = 0;
  };
  ShareCounters share_counters() const { return counters_; }
  void connected() {
    ++session_generation_;
    subscription_.reset();
    pending_submits_.clear();
    last_submit_id_.clear();
    counters_ = {};
    subscribed_ = false;
    connected_ = true;
    jobs_.apply({JobState::EventKind::Connected});
  }
  void disconnected() {
    ++session_generation_;
    jobs_.apply({JobState::EventKind::Disconnected});
    subscription_.reset();
    pending_submits_.clear();
    subscribed_ = false;
    connected_ = false;
  }
  bool subscribe(std::string_view line, std::string_view request_id) {
    auto reply = decode_subscribe_reply(line, request_id);
    if (!connected_ || !reply || !subscription_.accept(*reply, jobs_)) return false;
    subscribed_ = true;
    return true;
  }
  bool authorize(std::string_view line, std::string_view request_id) {
    if (!subscribed_) return false;
    auto event = decode_authorize_reply(line, request_id);
    if (!event || !jobs_.apply(*event)) return false;
    return true;
  }
  bool difficulty(std::string_view line) {
    if (!subscribed_) return false;
    auto event = decode_difficulty(line);
    return event && jobs_.apply(*event);
  }
  bool notify(std::string_view line) {
    if (!subscribed_) return false;
    auto event = decode_notify(line);
    if (!event || !jobs_.apply(*event)) return false;
    return true;
  }
  // Track a share only while its job is current. Once sent, its reply can
  // arrive after a new job or extranonce invalidates the old work.
  // IDs must increase within a connection, so late duplicate replies cannot
  // resolve a different share. A reconnect starts a new ID sequence.
  // No network request is emitted by this offline component.
  bool track_submit(std::string_view request_id, const JobSnapshot& job) {
    if (!connected_ || !jobs_.can_submit(job) || request_id.empty() ||
        ((request_id.size() > 1 && request_id.front() == '0') ||
        request_id.size() > 20 || pending_submits_.size() >= 4096)) return false;
    for (char c : request_id) if (c < '0' || c > '9') return false;
    if (!last_submit_id_.empty() &&
        (request_id.size() < last_submit_id_.size() ||
         (request_id.size() == last_submit_id_.size() && request_id <= last_submit_id_)))
      return false;
    auto inserted = pending_submits_.insert(std::string(request_id)).second;
    if (inserted) last_submit_id_ = request_id;
    return inserted;
  }
  // The transport must tag replies with the generation of the connection
  // that received them; request IDs alone are not unique across reconnects.
  std::optional<SubmitOutcome> submit_reply(std::string_view line,
                                            std::string_view request_id,
                                            std::uint64_t reply_generation) {
    if (!connected_ || reply_generation != session_generation_) return std::nullopt;
    auto it = pending_submits_.find(std::string(request_id));
    if (it == pending_submits_.end()) return std::nullopt;
    auto result = decode_submit_reply(line, request_id);
    if (result) {
      pending_submits_.erase(it);
      switch (*result) {
        case SubmitOutcome::Accepted: ++counters_.accepted; break;
        case SubmitOutcome::Rejected: ++counters_.rejected; break;
        case SubmitOutcome::PoolError: ++counters_.pool_errors; break;
      }
    }
    return result;
  }
  std::uint64_t session_generation() const { return session_generation_; }
  const JobState& jobs() const { return jobs_; }
 private:
  JobState jobs_;
  ShareCounters counters_;
  std::unordered_set<std::string> pending_submits_;
  std::string last_submit_id_;
  SubscribeWorkGuard subscription_;
  bool subscribed_ = false;
  bool connected_ = false;
  std::uint64_t session_generation_ = 0;
};
} // namespace pepepow
