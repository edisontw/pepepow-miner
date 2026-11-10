#include <iostream>
#include <string_view>
#include "../include/pepepow/local_endpoint.hpp"

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view(argv[1]) != "--check-local-endpoint") {
    std::cerr << "Usage: miner_preflight --check-local-endpoint 127.0.0.1:PORT|localhost:PORT\n";
    return 2;
  }
  unsigned port = 0;
  if (!pepepow::cli::parse_loopback_endpoint(argv[2], port)) {
    std::cerr << "Invalid endpoint: only 127.0.0.1:PORT or localhost:PORT (1-65535) is permitted\n";
    return 2;
  }
  std::cout << "{\"endpoint\":\"127.0.0.1:" << port
            << "\",\"network_opened\":false,\"g0\":\"NOT PASSED\",\"mining_ready\":false}\n";
  return 0;
}
