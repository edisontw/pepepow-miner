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
    // Invalid receive handles must not retire a healthy session or consume IDs.
    if (pepepow::socket_io::receive_controller_line(invalid_socket, controller, generation)) break;
    if (controller.session().session_generation() != generation) break;
    if (pepepow::socket_io::receive_controller_line(client, controller, generation + 1)) break;
    if (controller.session().session_generation() != generation) break;
    // Invalid handles must not reserve request IDs or retire a live generation.
    if (pepepow::socket_io::send_controller_subscribe(invalid_socket, controller, generation)) break;
    if (controller.session().session_generation() != generation) break;
    auto subscribe = controller.make_subscribe_request();
    if (!subscribe) break;
    auto pending = controller.pending_status();
    if (!pending.connected || !pending.subscribe_pending || pending.subscribed ||
        pending.authorize_pending || pending.submit_pending != 0 ||
        pending.generation != generation) break;
    // Invalid and stale receive attempts must preserve an outstanding request.
    if (pepepow::socket_io::receive_controller_line(invalid_socket, controller, generation)) break;
    if (pepepow::socket_io::receive_controller_line(client, controller, generation + 1)) break;
    if (controller.session().session_generation() != generation) break;
    if (controller.make_subscribe_request()) break; // Existing ID 1 is still pending.
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
    pending = controller.pending_status();
    if (pending.subscribe_pending || !pending.subscribed) break;
    // Invalid credentials and stale generation must not allocate an ID.
    if (pepepow::socket_io::send_controller_authorize(
            client, controller, generation, "bad\nlogin")) break;
    if (pepepow::socket_io::send_controller_authorize(
            client, controller, generation + 1, "WALLET.worker")) break;
    if (pepepow::socket_io::send_controller_authorize(
            invalid_socket, controller, generation, "WALLET.worker")) break;
    if (controller.session().session_generation() != generation) break;
    if (!pepepow::socket_io::send_controller_authorize(
            client, controller, generation, "WALLET.worker")) break;
    const std::string expected_authorize =
        R"({"id":2,"method":"mining.authorize","params":["WALLET.worker","x"]})" "\n";
    if (!receive_exact(peer, expected_authorize)) break;
    if (!controller.pending_status().authorize_pending) break;
    const std::string auth_reply = R"({"id":2,"result":true,"error":null})" "\n";
    if (!send_all(peer, auth_reply)) break;
    auto auth_events = pepepow::socket_io::receive_controller_line(client, controller, generation);
    if (!auth_events || auth_events->authorized != 1 ||
        controller.pending_status().authorize_pending) break;
    // Offline submit is permitted only for an authorized, current job.
    const std::string difficulty =
        R"({"id":null,"method":"mining.set_difficulty","params":[2]})" "\n";
    const std::string notify =
        std::string(R"({"id":null,"method":"mining.notify","params":["job1",")") +
        std::string(64, 'a') +
        R"(","","",[],"00004000","1d00ffff","67500000",true]})" "\n";
    if (!send_all(peer, difficulty + notify)) break;
    const auto work_events1 = pepepow::socket_io::receive_controller_line(
        client, controller, generation);
    const auto work_events2 = pepepow::socket_io::receive_controller_line(
        client, controller, generation);
    if (!work_events1 || !work_events2 ||
        work_events1->applied != 1 || work_events2->applied != 1) break;
    const auto active = controller.session().jobs().active();
    if (!active) break;
    if (pepepow::socket_io::send_controller_submit(
            client, controller, generation + 1, *active,
            "WALLET.worker", "00", 1, "67500000", "12345678")) break;
    if (pepepow::socket_io::send_controller_submit(
            client, controller, generation, *active,
            "WALLET.worker", "xx", 1, "67500000", "12345678")) break;
    if (pepepow::socket_io::send_controller_submit(
            invalid_socket, controller, generation, *active,
            "WALLET.worker", "00", 1, "67500000", "12345678")) break;
    if (controller.session().session_generation() != generation) break;
    if (!pepepow::socket_io::send_controller_submit(
            client, controller, generation, *active,
            "WALLET.worker", "00", 1, "67500000", "12345678")) break;
    if (!receive_exact(peer,
        R"({"id":3,"method":"mining.submit","params":["WALLET.worker","job1","00","67500000","12345678"]})" "\n")) break;
    if (!send_all(peer, R"({"id":3,"result":true,"error":null})" "\n")) break;
    const auto submit_events = pepepow::socket_io::receive_controller_line(
        client, controller, generation);
    if (!submit_events || submit_events->shares_accepted != 1 ||
        controller.pending_status().submit_pending != 0) break;
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
    if (!fresh || fresh->find("\"id\":4") == std::string::npos) break;
    // A stale generation must not read bytes belonging to the new session.
    const std::string fresh_reply = R"({"id":4,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n";
    // Malformed JSONL must not satisfy or consume the pending subscribe ID.
    if (!send_all(peer, R"({\"id\":4,\"result\":)" "\n")) break;
    const auto malformed_events = pepepow::socket_io::receive_controller_line(
        client, controller, controller.session().session_generation());
    if (!malformed_events || malformed_events->ignored != 1 ||
        malformed_events->subscribed != 0) break;
    if (controller.make_subscribe_request()) break; // ID 4 remains pending.
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
    // Failed authorize writes must discard their pending ID and generation.
    const auto failed_auth_generation = controller.session().session_generation();
    if (pepepow::socket_io::send_controller_authorize(
            client, controller, failed_auth_generation, "WALLET.worker")) break;
    if (controller.session().session_generation() == failed_auth_generation) break;
    if (controller.make_authorize_request("WALLET.worker") ||
        controller.pending_status().authorize_pending ||
        controller.pending_status().connected) break;
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
    if (!retry || retry->find("\"id\":7") == std::string::npos) break;
    // Oversized JSONL must fail closed and retire the pending subscribe ID.
    const auto oversized_generation = controller.session().session_generation();
    if (!send_all(peer, std::string(pepepow::socket_io::max_frame_bytes, 'x'))) break;
    if (!send_all(peer, "\n")) break;
    if (pepepow::socket_io::receive_controller_line(client, controller, oversized_generation)) break;
    if (controller.session().session_generation() == oversized_generation) break;
    if (controller.make_authorize_request("WALLET.worker")) break;
    // The oversized frame's trailing newline remains unread; drain it before
    // testing peer EOF on the same socket.
    std::string trailing;
    if (!pepepow::socket_io::receive_line(client, trailing) || trailing != "\n") break;
    controller.connected();
    auto after_oversized = controller.make_subscribe_request();
    if (!after_oversized || after_oversized->find("\"id\":8") == std::string::npos) break;
    // Peer EOF must retire pending subscribe ID and invalidate this generation.
    const auto eof_generation = controller.session().session_generation();
    close_socket(peer);
    peer = invalid_socket;
    if (pepepow::socket_io::receive_controller_line(client, controller, eof_generation)) break;
    if (controller.session().session_generation() == eof_generation) break;
    if (controller.make_authorize_request("WALLET.worker")) break;
    controller.connected();
    auto after_eof = controller.make_subscribe_request();
    if (!after_eof || after_eof->find("\"id\":9") == std::string::npos) break;
    // Deadline boundary must preserve the pending request until expiry.
    const auto timeout_generation = controller.session().session_generation();
    if (pepepow::socket_io::expire_controller_transport_request(
            client, controller, timeout_generation, 9, 100, 109, 10)) break;
    if (!controller.pending_status().subscribe_pending) break;
    if (pepepow::socket_io::expire_controller_transport_request(
            invalid_socket, controller, timeout_generation, 9, 100, 110, 10)) break;
    if (!pepepow::socket_io::expire_controller_transport_request(
            client, controller, timeout_generation, 9, 100, 110, 10)) break;
    if (controller.pending_status().connected ||
        controller.pending_status().subscribe_pending ||
        controller.session().session_generation() == timeout_generation) break;
    if (controller.receive_work_events(reply, timeout_generation).subscribed != 0) break;
    controller.connected();
    auto after_timeout = controller.make_subscribe_request();
    if (!after_timeout || after_timeout->find("\"id\":10") == std::string::npos) break;
    if (pepepow::socket_io::expire_controller_transport_request(
            client, controller, timeout_generation, 10, 20, 21, 1)) break;
    if (!controller.pending_status().subscribe_pending) break;
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
