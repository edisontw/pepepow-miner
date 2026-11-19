#include "pepepow/offline_stratum_controller.hpp"
#ifdef PEPEPOW_NATIVE_STRATUM_SEARCH
#include "pepepow/offline_stratum_search.hpp"
#endif
#include <array>
#include <iostream>
#include <sstream>
#include <string>

// Exercise a real controller disconnect/reconnect without opening a socket.
static int reconnect_self_test() {
  pepepow::OfflineStratumController controller;
  controller.connected();
  const auto old_generation = controller.session().session_generation();
  if (!controller.make_subscribe_request()) return 1;
  const auto partial = controller.receive_work_events(
      R"({"id":1,"result":[)", old_generation);
  if (partial.subscribed || partial.framing_rejected) return 1;
  controller.disconnected();
  controller.connected();
  const auto new_generation = controller.session().session_generation();
  if (new_generation == old_generation || !controller.make_subscribe_request()) return 1;
  const auto stale = controller.receive_work_events(
      R"({"id":1,"result":true,"error":null})" "\n", old_generation);
  if (stale.subscribed || stale.authorized || stale.applied || stale.ignored) return 1;
  const auto fresh = controller.receive_work_events(
      R"({"id":2,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n",
      new_generation);
  if (fresh.subscribed != 1 || fresh.ignored || fresh.framing_rejected) return 1;
  // A stale fragment from the previous connection must not corrupt the new frame.
  if (!controller.make_authorize_request("WALLET.worker")) return 1;
  const auto auth = controller.receive_work_events(
      R"({"id":3,"result":true,"error":null})" "\n", new_generation);
  return auth.authorized == 1 && auth.ignored == 0 && auth.framing_rejected == 0 ? 0 : 1;
}

// Verify cancelled request IDs are not reused within a connection.
static int cancellation_self_test() {
  pepepow::OfflineStratumController controller;
  controller.connected();
  const auto generation = controller.session().session_generation();
  const auto first = controller.make_subscribe_request();
  if (!first || first->find("\"id\":1") == std::string::npos) return 1;
  if (!controller.cancel_request(1, generation)) return 1;
  if (controller.cancel_request(1, generation)) return 1;
  const auto second = controller.make_subscribe_request();
  if (!second || second->find("\"id\":2") == std::string::npos) return 1;
  const auto stale = controller.receive_work_events(
      R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n",
      generation);
  if (stale.subscribed || stale.authorized) return 1;
  const auto fresh = controller.receive_work_events(
      R"({"id":2,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})" "\n",
      generation);
  if (fresh.subscribed != 1) return 1;
  controller.disconnected();
  controller.connected();
  return !controller.cancel_request(2, generation) &&
         controller.make_subscribe_request().has_value() ? 0 : 1;
}

int main(int argc, char** argv) {
#ifdef PEPEPOW_NATIVE_STRATUM_SEARCH
  std::uint32_t start = 0, count = 0;
  double scale = pepepow::OfflineStratumSearch::default_wire_difficulty_scale;
  const bool search = (argc == 6 || argc == 7) && std::string_view(argv[2]) == "--search";
  if (search) {
    const auto parse = [](std::string_view text, auto& number) {
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
      return error == std::errc{} && end == text.data() + text.size();
    };
    if (!parse(argv[3], start) || !parse(argv[4], count) ||
        !pepepow::valid_search_range(start, count)) {
      std::cerr << "Invalid nonce range\n";
      return 2;
    }
    if (argc == 7 && (!parse(argv[6], scale) || !std::isfinite(scale) || scale < 1.0)) {
      std::cerr << "Invalid wire difficulty scale\n";
      return 2;
    }
  }
#else
  constexpr bool search = false;
#endif
  // Machine-readable readiness for launchers. Never report mining as ready
  // while independent HooHash G0 parity and a live transport are absent.
  if (argc == 2 && std::string(argv[1]) == "--health-json") {
    std::cout << R"({"mode":"offline-replay","g0":"NOT PASSED","transport":"unavailable","mining_ready":false})" << '\n';
    return 0;
  }
  if ((!search && argc != 2) || std::string(argv[1]) == "--help") {
    std::cout << "Usage: offline_stratum_replay LOGIN < transcript.jsonl\n"
#ifdef PEPEPOW_NATIVE_STRATUM_SEARCH
                 "       offline_stratum_replay LOGIN --search START COUNT EXTRANONCE2_HEX [WIRE_DIFFICULTY_SCALE] < transcript.jsonl\n"
                 "Offline only: search emits a mock request to stdout. No sockets.\n";
#else
                 "Offline only: no socket, hashing or share submission.\n";
#endif
    return argc == 2 ? 0 : 2;
  }

  if (!search && std::string(argv[1]) == "--self-test-reconnect") return reconnect_self_test();
  if (!search && std::string(argv[1]) == "--self-test-cancel") return cancellation_self_test();
  const bool truncated_test = !search && std::string(argv[1]) == "--self-test-truncated";
  const bool malformed_test = !search && std::string(argv[1]) == "--self-test-malformed";
  const bool missing_authorize_test = !search && std::string(argv[1]) == "--self-test-missing-authorize";
  const bool missing_subscribe_test = !search && std::string(argv[1]) == "--self-test-missing-subscribe";
  const bool unexpected_test = !search && std::string(argv[1]) == "--self-test-unexpected";
  const bool self_test = (!search && std::string(argv[1]) == "--self-test") || truncated_test || malformed_test || unexpected_test || missing_authorize_test || missing_subscribe_test;
  // Unknown flags are not pool logins; fail before emitting a request frame.
  if ((!self_test || search) && argv[1][0] == '-') {
    std::cerr << "Unknown option: " << argv[1] << '\n';
    return 2;
  }
  const std::string login = self_test ? "WALLET.worker" : argv[1];
  // Reject invalid credentials before emitting any outbound subscribe frame.
  if (!pepepow::encode_authorize_request(1, login)) {
    std::cerr << "Invalid Stratum login\\n";
    return 2;
  }
  std::istringstream fixture(
      std::string(R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})") + "\n" +
      R"({"id":2,"result":true,"error":null})" "\n" +
      R"({"id":null,"method":"mining.set_difficulty","params":[2]})" "\n");
  if (missing_subscribe_test) {
    // EOF without a subscribe reply must leave the subscribe ID pending.
    fixture.str("");
    fixture.clear();
  }
  if (missing_authorize_test) {
    // A valid subscribe without an authorize reply must fail readiness.
    fixture.str(std::string(R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})") + "\n");
    fixture.clear();
  }
  if (truncated_test) {
    fixture.str(std::string(R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})") + "\n" +
                R"({"id":2,"result":true,"error":null})" "\n" +
                R"({"id":null,"method":"mining.set_difficulty","params":[2]})" "\n" +
                R"({"id":null,"method":"mining.notify","params":[)");
    fixture.clear();
  }
  if (malformed_test) {
    fixture.str(fixture.str() + std::string(65537, 'x') + "\n");
    fixture.clear();
  }
  if (unexpected_test) {
    fixture.str(fixture.str() + R"({"id":999,"result":true,"error":null})" "\n");
    fixture.clear();
  }
  std::istream& input = self_test ? static_cast<std::istream&>(fixture) : std::cin;
  pepepow::OfflineStratumController controller;
  controller.connected();
  const auto generation = controller.session().session_generation();
  const auto subscribe = controller.make_subscribe_request();
  if (!subscribe) {
    std::cerr << "Cannot construct subscribe request\n";
    return 2;
  }
  // Show the request that a future transport would send, without sending it.
  std::cout << *subscribe;
  std::array<char, 4096> buffer{};
  std::size_t applied = 0, ignored = 0, rejected = 0;
  std::size_t subscribed = 0, authorized = 0;
  bool line_terminated = true;
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    if (count <= 0) break;
    line_terminated = buffer[static_cast<std::size_t>(count) - 1] == '\n';
    // Process one completed line at a time: an authorization reply must not
    // race ahead of registration of the authorization request.
    std::string_view bytes(buffer.data(), static_cast<std::size_t>(count));
    while (!bytes.empty()) {
      const auto newline = bytes.find('\n');
      const auto length = newline == std::string_view::npos ? bytes.size() : newline + 1;
      const auto events = controller.receive_work_events(bytes.substr(0, length), generation);
      applied += events.applied;
      ignored += events.ignored;
      rejected += events.framing_rejected;
      subscribed += events.subscribed;
      authorized += events.authorized;
      if (events.subscribed) {
        auto request = controller.make_authorize_request(login);
        if (!request) {
          std::cerr << "Invalid login or authorization request\n";
          return 2;
        }
        std::cout << *request;
      }
      bytes.remove_prefix(length);
    }
  }
  if (!line_terminated) {
    std::cerr << "Transcript ends with an unterminated Stratum frame\\n";
    return 1;
  }
  if (input.bad()) {
    std::cerr << "Transcript read failed\n";
    return 2;
  }
  if (rejected != 0 || ignored != 0) {
    std::cerr << "Transcript contains rejected or unrecognized Stratum frames: "
              << rejected << "/" << ignored << '\n';
    return 1;
  }
  const auto pending = controller.pending_status();
  std::cerr << "offline_pending: generation=" << pending.generation
            << " connected=" << pending.connected
            << " subscribed=" << pending.subscribed
            << " subscribe=" << pending.subscribe_pending
            << " authorize=" << pending.authorize_pending
            << " submit=" << pending.submit_pending << '\n';
  const auto shares = controller.session().share_counters();
  std::cerr << "offline: subscribed=" << subscribed
            << " authorized=" << authorized
            << " work_events=" << applied
            << " ignored=" << ignored
            << " framing_rejected=" << rejected
            << " shares_accepted=" << shares.accepted
            << " shares_rejected=" << shares.rejected
            << " shares_pool_errors=" << shares.pool_errors << '\n';
  // A transcript is not healthy if request replies never cleared their IDs.
  // This also makes --self-test check the CLI pending-status lifecycle.
  const bool no_pending = !pending.subscribe_pending &&
                          !pending.authorize_pending && !pending.submit_pending;
  if (subscribed != 1 || authorized != 1 || !no_pending ||
      (self_test && applied != 1)) return 1;
#ifdef PEPEPOW_NATIVE_STRATUM_SEARCH
  if (search) {
    auto work = pepepow::OfflineStratumSearch::capture(controller, argv[5], scale);
    if (!work) {
      std::cerr << "No valid work for the supplied extranonce2\n";
      return 2;
    }
    const auto status = work->search(start, count);
    if (status == pepepow::SearchStatus::Timeout) {
      std::cerr << "status=timeout\n";
      return 4;
    }
    if (status == pepepow::SearchStatus::Exhausted) {
      std::cerr << "status=exhausted\n";
      return 1;
    }
    auto request = work->make_mock_submit(controller, login);
    if (!request) {
      std::cerr << "Cannot construct mock submit request\n";
      return 3;
    }
    std::cout << *request;
  }
#endif
  return 0;
}
