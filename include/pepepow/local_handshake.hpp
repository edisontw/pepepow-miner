#pragma once
#include "pepepow/offline_stratum_controller.hpp"
#include "pepepow/stratum_loopback_transport.hpp"
#include <cstdint>
#include <string_view>

namespace pepepow::cli {
// Explicit one-shot, numeric IPv4 loopback transport. No DNS, hashing, or shares.
inline bool perform_local_handshake(unsigned port, std::string_view login) {
  if (port == 0 || port > 65535 ||
      !pepepow::encode_authorize_request(1, login)) return false;
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
  using namespace pepepow::socket_io;
  const Socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  bool ok = false;
  if (s != invalid_socket) {
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(static_cast<std::uint16_t>(port));
    remote.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(s, reinterpret_cast<const sockaddr*>(&remote),
                  sizeof(remote)) == 0 && configure_timeouts(s)) {
      pepepow::OfflineStratumController controller;
      controller.connected();
      const auto generation = controller.session().session_generation();
      if (send_controller_subscribe(s, controller, generation)) {
        const auto sub = receive_controller_line(s, controller, generation);
        if (sub && sub->subscribed == 1 && sub->framing_rejected == 0 &&
            send_controller_authorize(s, controller, generation, login)) {
          const auto auth = receive_controller_line(s, controller, generation);
          ok = auth && auth->authorized == 1 && auth->framing_rejected == 0;
        }
      }
      controller.disconnected();
    }
    close_socket(s);
  }
#ifdef _WIN32
  WSACleanup();
#endif
  return ok;
}
}  // namespace pepepow::cli
