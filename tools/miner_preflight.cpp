#include <charconv>
#include <iostream>
#include <string_view>

// Offline-only CLI preflight: accepts loopback endpoints, never opens a socket.
static bool loopback_port(std::string_view endpoint, unsigned& port) {
  constexpr std::string_view prefix = "127.0.0.1:";
  if (!endpoint.starts_with(prefix)) return false;
  const auto digits = endpoint.substr(prefix.size());
  if (digits.empty() || digits.size() > 5) return false;
  for (const char ch : digits) if (ch < '0' || ch > '9') return false;
  unsigned value = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
  if (error != std::errc{} || end != digits.data() + digits.size() || value == 0 || value > 65535) return false;
  port = value;
  return true;
}

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--check-local-endpoint") {
    std::cerr << "Usage: miner_preflight --check-local-endpoint 127.0.0.1:PORT\n";
    return 2;
  }
  unsigned port = 0;
  if (!loopback_port(argv[2], port)) {
    std::cerr << "Invalid endpoint: only numeric 127.0.0.1:PORT (1-65535) is permitted\n";
    return 2;
  }
  std::cout << "{\"endpoint\":\"127.0.0.1:" << port
            << "\",\"network_opened\":false,\"g0\":\"NOT PASSED\",\"mining_ready\":false}\n";
  return 0;
}
