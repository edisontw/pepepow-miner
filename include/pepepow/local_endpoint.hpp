#pragma once
#include <charconv>
#include <string_view>

namespace pepepow::cli {
// Strict offline-only endpoint validation. No DNS resolution or socket creation.
// The returned port is always in host byte order.
inline bool parse_loopback_endpoint(std::string_view endpoint, unsigned& port) {
  constexpr std::string_view ipv4_prefix = "127.0.0.1:";
  constexpr std::string_view localhost_prefix = "localhost:";
  std::string_view digits;
  if (endpoint.starts_with(ipv4_prefix)) digits = endpoint.substr(ipv4_prefix.size());
  else if (endpoint.starts_with(localhost_prefix)) digits = endpoint.substr(localhost_prefix.size());
  else return false;
  if (digits.empty() || digits.size() > 5) return false;
  for (const char ch : digits) if (ch < '0' || ch > '9') return false;
  unsigned value = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
  if (error != std::errc{} || end != digits.data() + digits.size() || value == 0 || value > 65535) return false;
  port = value;
  return true;
}
} // namespace pepepow::cli
