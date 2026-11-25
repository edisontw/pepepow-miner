#pragma once
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <poll.h>
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
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
inline constexpr auto frame_budget = std::chrono::seconds(2);

enum class Readiness { Ready, Timeout, Error };

inline Readiness socket_ready(Socket s, int milliseconds, bool writing = false) {
#ifdef _WIN32
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(s, &readable);
  timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
  const int result = select(0, writing ? nullptr : &readable, writing ? &readable : nullptr,
                            nullptr, &timeout);
  if (result < 0 && WSAGetLastError() == WSAEINTR) return Readiness::Timeout;
#else
  pollfd readable{s, static_cast<short>(writing ? POLLOUT : POLLIN), 0};
  const int result = poll(&readable, 1, milliseconds);
  if (result < 0 && errno == EINTR) return Readiness::Timeout;
#endif
  if (result < 0) return Readiness::Error;
  return result == 0 ? Readiness::Timeout : Readiness::Ready;
}

// Used after the miner switches its connected socket to nonblocking mode.
inline bool temporarily_unavailable() {
#ifdef _WIN32
  const auto error = WSAGetLastError();
  return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
  return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

inline bool send_before(Socket s, std::string_view bytes, Deadline deadline,
                        const std::atomic_bool& stop) {
  if (bytes.empty() || bytes.size() > max_frame_bytes) return false;
  while (!bytes.empty() && !stop && Clock::now() < deadline) {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif
    const int n = send(s, bytes.data(), static_cast<int>(bytes.size()), flags);
    if (n > 0) {
      bytes.remove_prefix(static_cast<std::size_t>(n));
    } else if (n == 0 || !temporarily_unavailable() ||
               socket_ready(s, 1, true) == Readiness::Error) {
      return false;
    }
  }
  return bytes.empty() && !stop && Clock::now() < deadline;
}

// The caller owns the socket and is its sole reader. A partial frame cannot
// be reused after a failed read. Neither incoming bytes nor interrupted waits
// can extend the absolute deadline.
inline bool receive_line(Socket s, std::string& line, Deadline operation_deadline,
                         const std::atomic_bool* stop = nullptr,
                         Clock::duration budget = frame_budget) {
  line.clear();
  if (s == invalid_socket) return false;
  const auto deadline = (std::min)(operation_deadline, Clock::now() + budget);
  while (line.size() < max_frame_bytes && !(stop && *stop)) {
    const auto now = Clock::now();
    if (now >= deadline) break;
    auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
    if (stop) remaining = (std::min)(remaining, std::chrono::milliseconds(25));
    const auto ready = socket_ready(s, static_cast<int>(remaining.count()));
    if (ready == Readiness::Error) break;
    if (ready == Readiness::Timeout) continue;
    if (Clock::now() >= deadline || (stop && *stop)) break;
    char byte = 0;
#ifdef _WIN32
    const int n = recv(s, &byte, 1, 0);
#else
    const int n = recv(s, &byte, 1, MSG_DONTWAIT);
#endif
    if (n < 0 && temporarily_unavailable()) continue;
    if (n != 1 || Clock::now() >= deadline || (stop && *stop)) break;
    line.push_back(byte);
    if (byte == '\n') return true;
  }
  line.clear();
  return false;
}
inline bool receive_line(Socket s, std::string& line, int milliseconds = 2000) {
  return milliseconds > 0
      ? receive_line(s, line, (Deadline::max)(), nullptr, std::chrono::milliseconds(milliseconds))
      : (line.clear(), false);
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
