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
  if (!receive_line(socket, line)) return std::nullopt;
  return controller.receive_work_events(line, generation);
}

// Send a controller-owned subscribe request only for the active generation.
// Any socket write failure invalidates the session: a partial JSONL write
// cannot safely be retried on the same connection.
inline bool send_controller_subscribe(
    Socket socket, OfflineStratumController& controller, std::uint64_t generation) {
  if (generation != controller.session().session_generation()) return false;
  auto request = controller.make_subscribe_request();
  if (!request) return false;
  if (send_all(socket, *request)) return true;
  controller.disconnected();
  return false;
}
} // namespace pepepow::socket_io
