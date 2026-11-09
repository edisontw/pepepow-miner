#include "pepepow/offline_stratum_controller.hpp"
#include "pepepow/stratum_socket_io.hpp"
#include "pepepow/stratum_loopback_transport.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
using pepepow::socket_io::Socket;
using pepepow::socket_io::invalid_socket;
using pepepow::socket_io::close_socket;
using pepepow::socket_io::send_all;
using pepepow::socket_io::receive_exact;

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
    if (!pepepow::socket_io::configure_timeouts(client) ||
        !pepepow::socket_io::configure_timeouts(peer)) break;
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
    // Invalid credentials and stale generation must not allocate an ID.
    if (pepepow::socket_io::send_controller_authorize(
            client, controller, generation, "bad\nlogin")) break;
    if (pepepow::socket_io::send_controller_authorize(
            client, controller, generation + 1, "WALLET.worker")) break;
    if (!pepepow::socket_io::send_controller_authorize(
            client, controller, generation, "WALLET.worker")) break;
    const std::string expected_authorize =
        R"({"id":2,"method":"mining.authorize","params":["WALLET.worker","x"]})" "\n";
    if (!receive_exact(peer, expected_authorize)) break;
    const std::string auth_reply = R"({"id":2,"result":true,"error":null})" "\n";
    if (!send_all(peer, auth_reply)) break;
    auto auth_events = pepepow::socket_io::receive_controller_line(client, controller, generation);
    if (!auth_events || auth_events->authorized != 1) break;
    // A single TCP write can contain multiple JSONL frames; neither may be lost.
    const std::string lines = R"({"id":90,"result":true,"error":null})" "\n"
                              R"({"id":91,"result":false,"error":null})" "\n";
    if (!send_all(peer, lines)) break;
    std::string line;
    if (!pepepow::socket_io::receive_line(client, line) ||
        line != R"({"id":90,"result":true,"error":null})" "\n") break;
    if (!pepepow::socket_io::receive_line(client, line) ||
        line != R"({"id":91,"result":false,"error":null})" "\n") break;
    controller.disconnected();
    controller.connected();
    if (controller.receive_work_events(auth_reply, generation).authorized != 0) break;
    auto fresh = controller.make_subscribe_request();
    if (!fresh || fresh->find("\"id\":3") == std::string::npos) break;
    // A stale generation must not read bytes belonging to the new session.
    const std::string fresh_reply = R"({"id":3,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n";
    if (!send_all(peer, fresh_reply)) break;
    if (pepepow::socket_io::receive_controller_line(client, controller, generation)) break;
    const auto fresh_events = pepepow::socket_io::receive_controller_line(
        client, controller, controller.session().session_generation());
    if (!fresh_events || fresh_events->subscribed != 1) break;
    // A write to a locally shut-down socket must fail, not kill the process.
#ifdef _WIN32
    if (shutdown(client, SD_SEND) != 0) break;
#else
    if (shutdown(client, SHUT_WR) != 0) break;
#endif
    if (send_all(client, "must-fail\\n")) break;
    // A failed outbound request must invalidate the pending ID and generation.
    controller.disconnected();
    controller.connected();
    const auto failed_generation = controller.session().session_generation();
    if (pepepow::socket_io::send_controller_subscribe(
            client, controller, failed_generation)) break;
    if (controller.session().session_generation() == failed_generation) break;
    if (controller.make_subscribe_request()) break;
    controller.connected();
    auto retry = controller.make_subscribe_request();
    if (!retry || retry->find("\"id\":5") == std::string::npos) break;
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
