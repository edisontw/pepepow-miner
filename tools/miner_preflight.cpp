#include "pepepow/local_endpoint.hpp"
#include "pepepow/local_handshake.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  const bool preflight = argc == 3 &&
      std::string_view(argv[1]) == "--check-local-endpoint";
  const bool handshake = argc == 4 &&
      std::string_view(argv[1]) == "--local-handshake";
  if (!preflight && !handshake) {
    std::cerr << "Usage:\n"
                 "  miner_preflight --check-local-endpoint 127.0.0.1:PORT\n"
                 "  miner_preflight --local-handshake 127.0.0.1:PORT LOGIN\n"
                 "Local handshake does not mine or submit shares.\n";
    return 2;
  }
  unsigned port = 0;
  if (!pepepow::cli::parse_loopback_endpoint(argv[2], port)) {
    std::cerr << "Invalid endpoint: only 127.0.0.1:PORT or localhost:PORT (1-65535) is permitted\n";
    return 2;
  }
  if (handshake) {
    const std::string_view login(argv[3]);
    if (!pepepow::encode_authorize_request(1, login)) {
      std::cerr << "Invalid Stratum login\n";
      return 2;
    }
    if (!pepepow::cli::perform_local_handshake(port, login)) {
      std::cerr << "Local Stratum handshake failed (connect, subscribe or authorize)\n";
      return 1;
    }
    std::cout << "{\"endpoint\":\"127.0.0.1:" << port
              << "\",\"subscribed\":true,\"authorized\":true,"
                 "\"network_opened\":true,\"scope\":\"loopback-only\","
                 "\"g0\":\"NOT PASSED\",\"mining_ready\":false}\n";
    return 0;
  }
  std::cout << "{\"endpoint\":\"127.0.0.1:" << port
            << "\",\"network_opened\":false,\"g0\":\"NOT PASSED\",\"mining_ready\":false}\n";
  return 0;
}
