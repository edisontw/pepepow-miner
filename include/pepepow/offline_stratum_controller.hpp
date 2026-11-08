#pragma once
#include "pepepow/offline_stratum_session.hpp"
#include "pepepow/stratum_line_framer.hpp"
#include "pepepow/stratum_request_encoder.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace pepepow {
// Offline request correlation only. The caller must send requests and deliver
// replies from the corresponding transport generation; no socket is opened.
class OfflineStratumController {
 public:
  void connected() {
    framer_.reset();
    session_.connected();
    connected_ = true;
    subscribed_ = false;
    next_ = 1;
    subscribe_id_.reset();
    authorize_id_.reset();
  }
  void disconnected() {
    framer_.reset();
    session_.disconnected();
    connected_ = false;
    subscribed_ = false;
    subscribe_id_.reset();
    authorize_id_.reset();
  }
  // Bounded offline receive framing. The transport must supply bytes from
  // only the current connection; framing is cleared on every reconnect.
  StratumLineFramer::Result receive_bytes(std::string_view bytes,
                                         std::uint64_t generation) {
    if (!connected_ || generation != session_.session_generation()) return {};
    return framer_.feed(bytes);
  }
  // Feed complete framed lines into the existing fixed-dialect work decoders.
  // Replies remain explicitly correlated by request ID through reply methods.
  struct WorkEvents {
    std::size_t applied = 0;
    std::size_t ignored = 0;
    std::size_t framing_rejected = 0;
    std::size_t subscribed = 0;
    std::size_t authorized = 0;
  };
  WorkEvents receive_work_events(std::string_view bytes,
                                 std::uint64_t generation) {
    WorkEvents events;
    if (!connected_ || generation != session_.session_generation()) return events;
    auto framed = receive_bytes(bytes, generation);
    events.framing_rejected = framed.rejected;
    for (const auto& line : framed.lines) {
      // Correlate control replies before accepting work notifications.
      // The strict decoders reject wrong IDs and unrelated JSON-RPC replies.
      if (subscribe_reply(line, generation)) {
        ++events.subscribed;
      } else if (authorize_reply(line, generation)) {
        ++events.authorized;
      } else if (session_.difficulty(line) || session_.notify(line)) {
        ++events.applied;
      } else {
        ++events.ignored;
      }
    }
    return events;
  }
  std::optional<std::uint64_t> begin_subscribe() {
    if (!connected_ || subscribe_id_) return std::nullopt;
    subscribe_id_ = allocate();
    return subscribe_id_;
  }
  std::optional<std::uint64_t> begin_authorize() {
    if (!connected_ || authorize_id_ || !subscribed_) return std::nullopt;
    authorize_id_ = allocate();
    return authorize_id_;
  }
  // Atomically validate and register an outbound request. Invalid input must
  // never consume an ID or leave a pending request behind.
  std::optional<std::string> make_subscribe_request() {
    if (!connected_ || subscribe_id_) return std::nullopt;
    auto id = begin_subscribe();
    return id ? encode_subscribe_request(*id) : std::nullopt;
  }
  std::optional<std::string> make_authorize_request(
      std::string_view login, std::string_view password = "x") {
    if (!connected_ || !subscribed_ || authorize_id_ ||
        !encode_authorize_request(1, login, password)) return std::nullopt;
    auto id = begin_authorize();
    return id ? encode_authorize_request(*id, login, password) : std::nullopt;
  }
  std::optional<std::string> make_submit_request(
      const JobSnapshot& job, std::string_view login,
      std::string_view extranonce2, unsigned extranonce2_size,
      std::string_view ntime, std::string_view nonce) {
    if (!connected_ || !session_.jobs().can_submit(job) ||
        !encode_submit_request(1, login, job.id, extranonce2,
                               extranonce2_size, ntime, nonce)) return std::nullopt;
    auto id = begin_submit(job);
    return id ? encode_submit_request(*id, login, job.id, extranonce2,
                                      extranonce2_size, ntime, nonce) : std::nullopt;
  }
  bool subscribe_reply(std::string_view line, std::uint64_t generation) {
    if (!connected_ || generation != session_.session_generation() || !subscribe_id_) return false;
    if (!session_.subscribe(line, std::to_string(*subscribe_id_))) return false;
    subscribe_id_.reset();
    subscribed_ = true;
    return true;
  }
  bool authorize_reply(std::string_view line, std::uint64_t generation) {
    if (!connected_ || generation != session_.session_generation() || !authorize_id_) return false;
    if (!session_.authorize(line, std::to_string(*authorize_id_))) return false;
    authorize_id_.reset();
    return true;
  }
  // Caller may invoke this after a timeout or an explicit cancellation.
  // Consumed IDs are never reused within this connection.
  bool cancel_request(std::uint64_t id, std::uint64_t generation) {
    if (!connected_ || generation != session_.session_generation()) return false;
    if (subscribe_id_ && *subscribe_id_ == id) { subscribe_id_.reset(); return true; }
    if (authorize_id_ && *authorize_id_ == id) { authorize_id_.reset(); return true; }
    return session_.cancel_submit(std::to_string(id), generation);
  }
  bool difficulty(std::string_view line) { return session_.difficulty(line); }
  bool notify(std::string_view line) { return session_.notify(line); }
  std::optional<std::uint64_t> begin_submit(const JobSnapshot& job) {
    // A stale or unauthorized job must not consume a request ID.
    if (!connected_ || !session_.jobs().can_submit(job)) return std::nullopt;
    auto id = allocate();
    if (!id || !session_.track_submit(std::to_string(*id), job)) return std::nullopt;
    return id;
  }
  std::optional<SubmitOutcome> submit_reply(std::string_view line, std::uint64_t id,
                                           std::uint64_t generation) {
    return session_.submit_reply(line, std::to_string(id), generation);
  }
  const OfflineStratumSession& session() const { return session_; }
 private:
  std::optional<std::uint64_t> allocate() {
    if (next_ == 0) return std::nullopt;
    auto id = next_;
    next_ = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
    return id;
  }
  OfflineStratumSession session_;
  StratumLineFramer framer_;
  std::uint64_t next_ = 1;
  std::optional<std::uint64_t> subscribe_id_, authorize_id_;
  bool connected_ = false;
  bool subscribed_ = false;
};
} // namespace pepepow
