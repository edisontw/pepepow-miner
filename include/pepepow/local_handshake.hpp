#pragma once
#include "pepepow/offline_stratum_controller.hpp"
#include "pepepow/stratum_loopback_transport.hpp"
#include <cstdint>
#include <string_view>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#endif

namespace pepepow::cli {
// This socket is newly created and blocking. Bound connect itself, then restore
// blocking mode for the existing send/receive helpers. Never send on failure.
inline bool connect_local_with_timeout(pepepow::socket_io::Socket s,
                                       const sockaddr_in& remote,
                                       int milliseconds = 2000) {
  if (milliseconds <= 0) return false;
#ifdef _WIN32
  u_long nonblocking = 1;
  if (ioctlsocket(s, FIONBIO, &nonblocking) != 0) return false;
#else
  const int flags = fcntl(s, F_GETFL, 0);
  if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) != 0) return false;
#endif
  bool ok = ::connect(s, reinterpret_cast<const sockaddr*>(&remote),
                      sizeof(remote)) == 0;
#ifdef _WIN32
  if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
    fd_set writable, errors;
    FD_ZERO(&writable);
    FD_ZERO(&errors);
    FD_SET(s, &writable);
    FD_SET(s, &errors);
    timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
    ok = select(0, nullptr, &writable, &errors, &timeout) > 0;
#else
  if (!ok && errno == EINPROGRESS) {
    pollfd pending{s, POLLOUT, 0};
    ok = poll(&pending, 1, milliseconds) > 0;
#endif
    if (ok) {
      int error = 0;
#ifdef _WIN32
      int size = sizeof(error);
#else
      socklen_t size = sizeof(error);
#endif
      ok = getsockopt(s, SOL_SOCKET, SO_ERROR,
                      reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
    }
  }
#ifdef _WIN32
  nonblocking = 0;
  const bool restored = ioctlsocket(s, FIONBIO, &nonblocking) == 0;
#else
  const bool restored = fcntl(s, F_SETFL, flags) == 0;
#endif
  return ok && restored;
}

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
    if (connect_local_with_timeout(s, remote) && configure_timeouts(s)) {
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
