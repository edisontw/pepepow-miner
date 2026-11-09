#include "pepepow/offline_stratum_controller.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
static void close_socket(Socket s) { if (s != invalid_socket) closesocket(s); }
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using Socket = int;
constexpr Socket invalid_socket = -1;
static void close_socket(Socket s) { if (s != invalid_socket) close(s); }
#endif

// TCP is a byte stream: a request can arrive in multiple recv() calls.
static bool receive_exact(Socket socket, std::string_view expected) {
  std::string received(expected.size(), '\0');
  std::size_t offset = 0;
  while (offset < received.size()) {
    const int count = recv(socket, received.data() + offset,
                           static_cast<int>(received.size() - offset), 0);
    if (count <= 0) return false;
    offset += static_cast<std::size_t>(count);
  }
  return received == expected;
}

// A TCP send may write fewer bytes than requested; never treat that as a complete request.
static bool send_all(Socket socket, std::string_view bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const int count = send(socket, bytes.data() + offset,
                           static_cast<int>(bytes.size() - offset), 0);
    if (count <= 0) return false;
    offset += static_cast<std::size_t>(count);
  }
  return true;
}

// Loopback-only wire smoke test. No miner hashing or external pool connection.
int main() {
#ifdef _WIN32
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
#endif
  int result = 1;
  Socket listener = invalid_socket, client = invalid_socket, peer = invalid_socket;
  do {
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) break;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0; // OS-selected ephemeral port, never a public listener.
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) break;
#ifdef _WIN32
    int address_size = sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size) != 0) break;
    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == invalid_socket) break;
    if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) break;
    peer = accept(listener, nullptr, nullptr);
    if (peer == invalid_socket) break;
#ifdef _WIN32
    const DWORD timeout = 2000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    const timeval timeout{2, 0};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    pepepow::OfflineStratumController controller;
    controller.connected();
    auto generation = controller.session().session_generation();
    auto subscribe = controller.make_subscribe_request();
    if (!subscribe) break;
    if (!send_all(client, *subscribe)) break;
    std::array<char, 512> buffer{};
    if (!receive_exact(peer, *subscribe)) break;
    const std::string reply = R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n";
    // Fragment the server reply across two TCP writes.
    const auto midpoint = reply.size() / 2;
    if (!send_all(peer, std::string_view(reply).substr(0, midpoint))) break;
    int count = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (count <= 0 || controller.receive_work_events(
        std::string_view(buffer.data(), static_cast<std::size_t>(count)), generation).subscribed != 0) break;
    if (!send_all(peer, std::string_view(reply).substr(midpoint))) break;
    count = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (count <= 0 || controller.receive_work_events(
        std::string_view(buffer.data(), static_cast<std::size_t>(count)), generation).subscribed != 1) break;
    auto authorize = controller.make_authorize_request("WALLET.worker");
    if (!authorize) break;
    if (!send_all(client, *authorize)) break;
    if (!receive_exact(peer, *authorize)) break;
    const std::string auth_reply = R"({"id":2,"result":true,"error":null})" "\n";
    if (!send_all(peer, auth_reply)) break;
    count = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (count <= 0 || controller.receive_work_events(
        std::string_view(buffer.data(), static_cast<std::size_t>(count)), generation).authorized != 1) break;
    controller.disconnected();
    controller.connected();
    if (controller.receive_work_events(auth_reply, generation).authorized != 0) break;
    auto fresh = controller.make_subscribe_request();
    if (!fresh || fresh->find("\"id\":3") == std::string::npos) break;
    result = 0;
  } while (false);
  close_socket(peer);
  close_socket(client);
  close_socket(listener);
#ifdef _WIN32
  WSACleanup();
#endif
  return result;
}
