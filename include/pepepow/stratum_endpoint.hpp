#pragma once

#include "pepepow/stratum_socket_io.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

namespace pepepow::socket_io {

struct ResolveState {
  std::mutex mutex;
  std::condition_variable finished;
  bool done = false;
  bool abandoned = false;
  int status = -1;
  addrinfo* addresses = nullptr;
};

inline std::atomic_bool resolver_in_flight{false};

inline std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resolve_with_timeout(
    std::string host, std::string service, addrinfo hints,
    std::chrono::milliseconds timeout) {
  if (timeout.count() <= 0 || resolver_in_flight.exchange(true))
    return {nullptr, &freeaddrinfo};
  auto state = std::make_shared<ResolveState>();
  try {
    std::thread([state, host = std::move(host), service = std::move(service), hints]() {
      addrinfo* result = nullptr;
      const int status = getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
      bool release_result = false;
      {
        std::lock_guard lock(state->mutex);
        state->done = true;
        state->status = status;
        if (state->abandoned || status != 0) release_result = true;
        else state->addresses = result;
      }
      resolver_in_flight.store(false);
      if (release_result && result) freeaddrinfo(result);
      state->finished.notify_one();
    }).detach();
  } catch (...) {
    resolver_in_flight.store(false);
    return {nullptr, &freeaddrinfo};
  }

  std::unique_lock lock(state->mutex);
  if (!state->finished.wait_for(lock, timeout, [&] { return state->done; })) {
    state->abandoned = true;
    return {nullptr, &freeaddrinfo};
  }
  if (state->status != 0 || !state->addresses)
    return {nullptr, &freeaddrinfo};
  return {state->addresses, &freeaddrinfo};
}

struct StratumTcpEndpoint {
  std::string host;
  std::uint16_t port = 0;
};

inline bool valid_dns_name(std::string_view host) {
  if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.')
    return false;
  std::size_t label_start = 0;
  while (label_start < host.size()) {
    const auto separator = host.find('.', label_start);
    const auto label_end = separator == std::string_view::npos ? host.size() : separator;
    const auto label = host.substr(label_start, label_end - label_start);
    if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-')
      return false;
    for (const unsigned char ch : label) {
      if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-')) return false;
    }
    if (separator == std::string_view::npos) break;
    label_start = separator + 1;
  }
  return true;
}

inline bool valid_ipv6_literal(std::string_view host) {
  if (host.empty()) return false;
  bool has_colon = false;
  for (const unsigned char ch : host) {
    if (ch == ':') has_colon = true;
    else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
               (ch >= 'A' && ch <= 'F') || ch == '.')) return false;
  }
  if (!has_colon) return false;
  const std::string value(host);
  std::array<unsigned char, 16> address{};
#ifdef _WIN32
  return InetPtonA(AF_INET6, value.c_str(), address.data()) == 1;
#else
  return inet_pton(AF_INET6, value.c_str(), address.data()) == 1;
#endif
}

// Accept only cleartext Stratum TCP URIs. TLS and URL credentials are rejected
// instead of being silently interpreted as plaintext endpoints.
inline bool parse_stratum_tcp_endpoint(std::string_view uri,
                                       StratumTcpEndpoint& endpoint) {
  constexpr std::string_view scheme = "stratum+tcp://";
  if (!uri.starts_with(scheme)) return false;
  auto authority = uri.substr(scheme.size());
  if (authority.empty() || authority.find_first_of("/?#@\\ \t\r\n") !=
                                std::string_view::npos) return false;

  std::string_view host;
  std::string_view port_text;
  if (authority.front() == '[') {
    const auto closing = authority.find(']');
    if (closing == std::string_view::npos || closing + 1 >= authority.size() ||
        authority[closing + 1] != ':') return false;
    host = authority.substr(1, closing - 1);
    port_text = authority.substr(closing + 2);
    if (!valid_ipv6_literal(host)) return false;
  } else {
    const auto separator = authority.rfind(':');
    if (separator == std::string_view::npos ||
        authority.find(':') != separator) return false;
    host = authority.substr(0, separator);
    port_text = authority.substr(separator + 1);
    if (!valid_dns_name(host)) return false;
    const bool numeric_ipv4 = std::all_of(host.begin(), host.end(), [](char ch) {
      return (ch >= '0' && ch <= '9') || ch == '.';
    });
    if (numeric_ipv4) {
      const std::string value(host);
      std::array<unsigned char, 4> address{};
#ifdef _WIN32
      if (InetPtonA(AF_INET, value.c_str(), address.data()) != 1) return false;
#else
      if (inet_pton(AF_INET, value.c_str(), address.data()) != 1) return false;
#endif
    }
  }

  if (port_text.empty() || port_text.size() > 5) return false;
  for (const char ch : port_text) if (ch < '0' || ch > '9') return false;
  unsigned port = 0;
  const auto [end, error] = std::from_chars(
      port_text.data(), port_text.data() + port_text.size(), port);
  if (error != std::errc{} || end != port_text.data() + port_text.size() ||
      port == 0 || port > 65535) return false;

  endpoint.host.assign(host);
  endpoint.port = static_cast<std::uint16_t>(port);
  return true;
}

inline std::string format_stratum_tcp_endpoint(const StratumTcpEndpoint& endpoint) {
  const auto host = endpoint.host.find(':') == std::string::npos
      ? endpoint.host : "[" + endpoint.host + "]";
  return "stratum+tcp://" + host + ":" + std::to_string(endpoint.port);
}

inline bool set_blocking_mode(Socket socket, bool blocking) {
#ifdef _WIN32
  u_long mode = blocking ? 0UL : 1UL;
  return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
  const int flags = fcntl(socket, F_GETFL, 0);
  if (flags < 0) return false;
  const int next = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
  return fcntl(socket, F_SETFL, next) == 0;
#endif
}

inline bool connect_address_with_timeout(Socket socket, const sockaddr* address,
                                         std::size_t address_length,
                                         std::chrono::milliseconds timeout) {
  if (socket == invalid_socket || !address || address_length == 0 || timeout.count() <= 0 ||
      !set_blocking_mode(socket, false)) return false;

#ifdef _WIN32
  const int length = static_cast<int>(address_length);
#else
  const auto length = static_cast<socklen_t>(address_length);
#endif
  bool connected = ::connect(socket, address, length) == 0;
  if (!connected) {
#ifdef _WIN32
    const auto error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY) {
      fd_set writable, errors;
      FD_ZERO(&writable);
      FD_ZERO(&errors);
      FD_SET(socket, &writable);
      FD_SET(socket, &errors);
      timeval wait{static_cast<long>(timeout.count() / 1000),
                   static_cast<long>((timeout.count() % 1000) * 1000)};
      const auto ready = select(0, nullptr, &writable, &errors, &wait);
      connected = ready > 0 && !FD_ISSET(socket, &errors) && FD_ISSET(socket, &writable);
    }
#else
    if (errno == EINPROGRESS) {
      pollfd pending{socket, POLLOUT, 0};
      const auto ready = poll(&pending, 1, static_cast<int>(timeout.count()));
      connected = ready > 0 && (pending.revents & POLLOUT) != 0 &&
                  (pending.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0;
    }
#endif
    if (connected) {
      int error = 0;
#ifdef _WIN32
      int size = sizeof(error);
#else
      socklen_t size = sizeof(error);
#endif
      connected = getsockopt(socket, SOL_SOCKET, SO_ERROR,
                             reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
    }
  }
  const bool restored = set_blocking_mode(socket, true);
  return connected && restored;
}

// Resolve once per connection attempt and try each returned address within one
// shared budget. The OS resolver runs off-thread and cannot block the miner past
// its resolution deadline; at most one resolver request may remain outstanding.
inline Socket connect_stratum_tcp_endpoint(
    const StratumTcpEndpoint& endpoint,
    std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  if (endpoint.host.empty() || endpoint.port == 0 || timeout.count() <= 0)
    return invalid_socket;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  const auto deadline = Clock::now() + timeout;
  const auto resolve_budget = (std::max)(std::chrono::milliseconds(1), timeout / 2);
  auto addresses = resolve_with_timeout(endpoint.host, std::to_string(endpoint.port),
                                        hints, resolve_budget);
  if (!addresses) return invalid_socket;
  for (auto* address = addresses.get(); address && Clock::now() < deadline;
       address = address->ai_next) {
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
    const Socket socket = ::socket(address->ai_family, address->ai_socktype,
                                   address->ai_protocol);
    if (socket == invalid_socket) continue;
    if (connect_address_with_timeout(socket, address->ai_addr,
                                     static_cast<std::size_t>(address->ai_addrlen), remaining))
      return socket;
    close_socket(socket);
  }
  return invalid_socket;
}

}  // namespace pepepow::socket_io
