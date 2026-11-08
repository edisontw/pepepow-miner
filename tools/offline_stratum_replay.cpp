#include "pepepow/offline_stratum_controller.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2 || std::string(argv[1]) == "--help") {
    std::cout << "Usage: offline_stratum_replay LOGIN < transcript.jsonl\n"
                 "Offline only: no socket, hashing or share submission.\n";
    return argc == 2 ? 0 : 2;
  }

  const std::string login = argv[1];
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
  std::string line;
  std::size_t applied = 0, ignored = 0, rejected = 0;
  std::size_t subscribed = 0, authorized = 0;
  while (std::getline(std::cin, line)) {
    auto events = controller.receive_work_events(line + "\n", generation);
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
  }
  if (std::cin.bad()) {
    std::cerr << "Transcript read failed\n";
    return 2;
  }
  const auto shares = controller.session().share_counters();
  std::cerr << "offline: subscribed=" << subscribed
            << " authorized=" << authorized
            << " work_events=" << applied
            << " ignored=" << ignored
            << " framing_rejected=" << rejected
            << " shares_accepted=" << shares.accepted
            << " shares_rejected=" << shares.rejected
            << " shares_pool_errors=" << shares.pool_errors << '\n';
  return (subscribed == 1 && authorized == 1) ? 0 : 1;
}
