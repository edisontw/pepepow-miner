#pragma once
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace pepepow::socket_io {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
inline void close_socket(Socket s) { if (s != invalid_socket) closesocket(s); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
inline void close_socket(Socket s) { if (s != invalid_socket) close(s); }
#endif

// Stratum JSONL is capped at 64 KiB; never send an unbounded frame.
constexpr std::size_t max_frame_bytes = 65536;
inline bool configure_timeouts(Socket s, int milliseconds = 2000) {
  if (s == invalid_socket || milliseconds <= 0) return false;
#ifdef _WIN32
  const DWORD timeout = static_cast<DWORD>(milliseconds);
#else
  const timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
#endif
  return setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0 &&
         setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
}
inline bool send_all(Socket s, std::string_view bytes) {
  if (s == invalid_socket || bytes.empty() || bytes.size() > max_frame_bytes) return false;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    // A disconnected Linux peer must not terminate the miner with SIGPIPE.
#ifdef MSG_NOSIGNAL
    constexpr int send_flags = MSG_NOSIGNAL;
#else
    constexpr int send_flags = 0;
#endif
    const int n = send(s, bytes.data() + offset,
                       static_cast<int>(bytes.size() - offset), send_flags);
    if (n <= 0) return false;
    offset += static_cast<std::size_t>(n);
  }
  return true;
}
// Read exactly one newline-terminated JSONL frame without consuming the next frame.
// On timeout, EOF or oversized input, discard the partial frame.
inline bool receive_line(Socket s, std::string& line) {
  line.clear();
  if (s == invalid_socket) return false;
  char byte = 0;
  while (line.size() < max_frame_bytes) {
    const int n = recv(s, &byte, 1, 0);
    if (n != 1) { line.clear(); return false; }
    line.push_back(byte);
    if (byte == '\n') return true;
  }
  line.clear();
  return false;
}
inline bool receive_exact(Socket s, std::string_view expected) {
  if (s == invalid_socket || expected.empty() || expected.size() > max_frame_bytes) return false;
  std::string received(expected.size(), '\0');
  std::size_t offset = 0;
  while (offset < received.size()) {
    const int n = recv(s, received.data() + offset,
                       static_cast<int>(received.size() - offset), 0);
    if (n <= 0) return false;
    offset += static_cast<std::size_t>(n);
  }
  return received == expected;
}
} // namespace pepepow::socket_io
