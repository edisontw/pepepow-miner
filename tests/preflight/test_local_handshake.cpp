#include "pepepow/local_handshake.hpp"
#include <string>
#include <thread>
using pepepow::socket_io::Socket;
using pepepow::socket_io::invalid_socket;
using pepepow::socket_io::close_socket;
using pepepow::socket_io::receive_line;
using pepepow::socket_io::send_all;

// OS-assigned loopback listener; validates real JSONL subscribe + authorize.
int main() {
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif
  const Socket listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == invalid_socket) return 1;
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  local.sin_port = 0;
  if (::bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0 ||
      ::listen(listener, 1) != 0) { close_socket(listener); return 1; }
#ifdef _WIN32
  int size = sizeof(local);
#else
  socklen_t size = sizeof(local);
#endif
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&local), &size) != 0) {
    close_socket(listener);
    return 1;
  }
  bool served = false;
  std::thread server([&] {
    const Socket peer = ::accept(listener, nullptr, nullptr);
    if (peer == invalid_socket) return;
    do {
      if (!pepepow::socket_io::configure_timeouts(peer)) break;
      std::string line;
      if (!receive_line(peer, line) ||
          line != R"({"id":1,"method":"mining.subscribe","params":[]})" "\n") break;
      if (!send_all(peer,
          R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n")) break;
      if (!receive_line(peer, line) ||
          line != R"({"id":2,"method":"mining.authorize","params":["WALLET.worker","x"]})" "\n") break;
      if (!send_all(peer, R"({"id":2,"result":true,"error":null})" "\n")) break;
      served = true;
    } while (false);
    close_socket(peer);
  });
  const auto port = static_cast<unsigned>(ntohs(local.sin_port));
  const bool ok = pepepow::cli::perform_local_handshake(port, "WALLET.worker");
  server.join();
  close_socket(listener);
  const bool invalid = !pepepow::cli::perform_local_handshake(0, "WALLET.worker") &&
      !pepepow::cli::perform_local_handshake(port, "bad\nlogin");
#ifdef _WIN32
  WSACleanup();
#endif
  return ok && served && invalid ? 0 : 1;
}
