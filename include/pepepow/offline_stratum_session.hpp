#pragma once
#include "pepepow/stratum_difficulty_decoder.hpp"
#include "pepepow/stratum_notify_decoder.hpp"
#include "pepepow/stratum_subscribe_decoder.hpp"
#include "pepepow/stratum_submit_decoder.hpp"
#include <string>
#include <unordered_set>
#include <string_view>

namespace pepepow {
// Offline fixed-dialect dispatcher. No socket, hashing, or share submission.
// A session must subscribe and authorize before accepting mining work.
class OfflineStratumSession {
 public:
  void connected() {
    subscription_.reset();
    pending_submits_.clear();
    subscribed_ = false;
    connected_ = true;
    jobs_.apply({JobState::EventKind::Connected});
  }
  void disconnected() {
    jobs_.apply({JobState::EventKind::Disconnected});
    subscription_.reset();
    pending_submits_.clear();
    subscribed_ = false;
    connected_ = false;
  }
  bool subscribe(std::string_view line, std::string_view request_id) {
    auto reply = decode_subscribe_reply(line, request_id);
    if (!connected_ || !reply || !subscription_.accept(*reply, jobs_)) return false;
    if (!jobs_.active()) pending_submits_.clear();
    subscribed_ = true;
    return true;
  }
  bool authorize(std::string_view line, std::string_view request_id) {
    if (!subscribed_) return false;
    auto event = decode_authorize_reply(line, request_id);
    if (!event || !jobs_.apply(*event)) return false;
    if (!event->authorized) pending_submits_.clear();
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
    pending_submits_.clear();
    return true;
  }
  // Only account for a response to an explicitly tracked, still-current share.
  // No network request is emitted by this offline component.
  bool track_submit(std::string_view request_id, const JobSnapshot& job) {
    if (!connected_ || !jobs_.can_submit(job) || request_id.empty() ||
        ((request_id.size() > 1 && request_id.front() == '0') ||
        request_id.size() > 20 || pending_submits_.size() >= 4096)) return false;
    for (char c : request_id) if (c < '0' || c > '9') return false;
    return pending_submits_.insert(std::string(request_id)).second;
  }
  std::optional<SubmitOutcome> submit_reply(std::string_view line,
                                            std::string_view request_id) {
    auto it = pending_submits_.find(std::string(request_id));
    if (it == pending_submits_.end()) return std::nullopt;
    auto result = decode_submit_reply(line, request_id);
    if (result) pending_submits_.erase(it);
    return result;
  }
  const JobState& jobs() const { return jobs_; }
 private:
  JobState jobs_;
  std::unordered_set<std::string> pending_submits_;
  SubscribeWorkGuard subscription_;
  bool subscribed_ = false;
  bool connected_ = false;
};
} // namespace pepepow
