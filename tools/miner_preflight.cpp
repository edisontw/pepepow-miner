#include <charconv>
#include <iostream>
#include <string_view>

// Offline-only CLI preflight: accepts loopback endpoints, never opens a socket.
static bool loopback_port(std::string_view endpoint, unsigned& port) {
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

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--check-local-endpoint") {
    std::cerr << "Usage: miner_preflight --check-local-endpoint 127.0.0.1:PORT|localhost:PORT\n";
    return 2;
  }
  unsigned port = 0;
  if (!loopback_port(argv[2], port)) {
    std::cerr << "Invalid endpoint: only 127.0.0.1:PORT or localhost:PORT (1-65535) is permitted\n";
    return 2;
  }
  std::cout << "{\"endpoint\":\"127.0.0.1:" << port
            << "\",\"network_opened\":false,\"g0\":\"NOT PASSED\",\"mining_ready\":false}\n";
  return 0;
}
