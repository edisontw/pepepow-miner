#pragma once
#include "pepepow/stratum_difficulty_decoder.hpp"
#include "pepepow/stratum_notify_decoder.hpp"
#include "pepepow/stratum_subscribe_decoder.hpp"
#include <string_view>

namespace pepepow {
// Offline fixed-dialect dispatcher. No socket, hashing, or share submission.
// A session must subscribe and authorize before accepting mining work.
class OfflineStratumSession {
 public:
  void connected() {
    subscription_.reset();
    subscribed_ = false;
    connected_ = true;
    jobs_.apply({JobState::EventKind::Connected});
  }
  void disconnected() {
    jobs_.apply({JobState::EventKind::Disconnected});
    subscription_.reset();
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
    return event && jobs_.apply(*event);
  }
  bool difficulty(std::string_view line) {
    if (!subscribed_) return false;
    auto event = decode_difficulty(line);
    return event && jobs_.apply(*event);
  }
  bool notify(std::string_view line) {
    if (!subscribed_) return false;
    auto event = decode_notify(line);
    return event && jobs_.apply(*event);
  }
  const JobState& jobs() const { return jobs_; }
 private:
  JobState jobs_;
  SubscribeWorkGuard subscription_;
  bool subscribed_ = false;
  bool connected_ = false;
};
} // namespace pepepow
