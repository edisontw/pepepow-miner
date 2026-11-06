#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace pepepow {
// Offline Stratum work lifecycle only. No networking, hashing or submitting.
struct JobSnapshot {
  std::string id;
  std::uint64_t generation;
  double difficulty;
};

class JobState {
 public:
  void connected() { ++generation_; authorized_ = false; active_.reset(); }
  void disconnected() { ++generation_; authorized_ = false; active_.reset(); }
  void authorized(bool ok) { authorized_ = ok; if (!ok) { ++generation_; active_.reset(); } }
  bool set_difficulty(double value) {
    if (!(value > 0.0) || !finite(value)) return false;
    difficulty_ = value;
    return true;
  }
  bool notify(std::string id, bool clean_jobs) {
    if (!authorized_ || id.empty()) return false;
    if (clean_jobs) ++generation_;
    active_ = JobSnapshot{std::move(id), generation_, difficulty_};
    return true;
  }
  std::optional<JobSnapshot> active() const { return active_; }
  bool can_submit(const JobSnapshot& job) const {
    return authorized_ && active_ && job.generation == generation_ &&
           job.generation == active_->generation && job.id == active_->id;
  }
 private:
  static bool finite(double value) { return value <= 1.7976931348623157e308; }
  std::uint64_t generation_ = 0;
  double difficulty_ = 1.0;
  bool authorized_ = false;
  std::optional<JobSnapshot> active_;
};
}  // namespace pepepow
