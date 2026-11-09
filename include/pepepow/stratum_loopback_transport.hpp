#pragma once
#include "pepepow/offline_stratum_controller.hpp"
#include "pepepow/stratum_socket_io.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace pepepow::socket_io {
// Consume at most one bounded JSONL frame from the current transport generation.
// A stale caller must not consume bytes from a newly connected socket.
inline std::optional<OfflineStratumController::WorkEvents> receive_controller_line(
    Socket socket, OfflineStratumController& controller, std::uint64_t generation) {
  if (generation != controller.session().session_generation()) return std::nullopt;
  std::string line;
  if (!receive_line(socket, line)) {
    // A failed read leaves outstanding replies unknowable; retire this session.
    controller.disconnected();
    return std::nullopt;
  }
  return controller.receive_work_events(line, generation);
}

// Send a controller-owned subscribe request only for the active generation.
// Any socket write failure invalidates the session: a partial JSONL write
// cannot safely be retried on the same connection.
inline bool send_controller_subscribe(
    Socket socket, OfflineStratumController& controller, std::uint64_t generation) {
  if (socket == invalid_socket || generation != controller.session().session_generation()) return false;
  auto request = controller.make_subscribe_request();
  if (!request) return false;
  if (send_all(socket, *request)) return true;
  controller.disconnected();
  return false;
}

// Authorize is permitted only after the current session's subscribe reply.
// Never retain a pending authorize ID after a partial/failed socket write.
inline bool send_controller_authorize(
    Socket socket, OfflineStratumController& controller, std::uint64_t generation,
    std::string_view login, std::string_view password = "x") {
  if (socket == invalid_socket || generation != controller.session().session_generation()) return false;
  auto request = controller.make_authorize_request(login, password);
  if (!request) return false;
  if (send_all(socket, *request)) return true;
  controller.disconnected();
  return false;
}

// Offline-only submit transport. The controller validates the active job,
// authorization and encoded fields before reserving a request ID. A failed
// write invalidates all pending IDs; never retry a partial JSONL frame.
inline bool send_controller_submit(
    Socket socket, OfflineStratumController& controller, std::uint64_t generation,
    const JobSnapshot& job, std::string_view login,
    std::string_view extranonce2, unsigned extranonce2_size,
    std::string_view ntime, std::string_view nonce) {
  if (socket == invalid_socket || generation != controller.session().session_generation()) return false;
  auto request = controller.make_submit_request(
      job, login, extranonce2, extranonce2_size, ntime, nonce);
  if (!request) return false;
  if (send_all(socket, *request)) return true;
  controller.disconnected();
  return false;
}
} // namespace pepepow::socket_io
