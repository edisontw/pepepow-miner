#include "pepepow/offline_stratum_search.hpp"

#include <iostream>
#include <source_location>

namespace {
constexpr std::string_view login = "mock.worker";
constexpr std::string_view extranonce2 = "12345678";
constexpr std::string_view notify =
    R"({"id":null,"method":"mining.notify","params":["mock-job","000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f","0102","0304",["202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"],"20004000","1d02e371","7169e10b",false]})" "\n";
constexpr std::string_view difficulty =
    R"({"id":null,"method":"mining.set_difficulty","params":[0.00005]})" "\n";

bool prepare(pepepow::OfflineStratumController& controller, unsigned first_id) {
  controller.connected();
  const auto generation = controller.session().session_generation();
  if (!controller.make_subscribe_request()) return false;
  const auto subscribe = "{\"id\":" + std::to_string(first_id) +
      R"(,"result":[[["mining.set_difficulty","mock"],["mining.notify","mock"]],"aabb",4],"error":null})" "\n";
  if (controller.receive_work_events(subscribe, generation).subscribed != 1 ||
      !controller.make_authorize_request(login)) return false;
  const auto authorize = "{\"id\":" + std::to_string(first_id + 1) +
      R"(,"result":true,"error":null})" "\n";
  return controller.receive_work_events(authorize, generation).authorized == 1 &&
         controller.receive_work_events(difficulty, generation).applied == 1 &&
         controller.receive_work_events(notify, generation).applied == 1;
}

std::string hex(const auto& bytes) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string text;
  for (const auto byte : bytes) {
    text += digits[byte >> 4];
    text += digits[byte & 15];
  }
  return text;
}

bool fixture_matches(const pepepow::CpuCandidate* found) {
  // Independently computed with hashlib SHA256d and g0_v110_independent.v110.
  constexpr std::string_view header =
      "0040002003020100070605040b0a09080f0e0d0c13121110171615141b1a19181f1e1d1c"
      "5b5d507845ead90ea4c4caeef4baef7817c0fb2169d20739883e22bb6d8cf8ef0be1697171e3021d06000000";
  constexpr std::string_view digest =
      "a0cba391e2eda64e067cf2c4f2209370bae8b31ac7ff4cc6cf141ebb25073a15";
  return found && found->nonce == 6 && hex(found->header) == header &&
         hex(found->digest) == digest;
}

bool failure(std::source_location where = std::source_location::current()) {
  std::cerr << where.file_name() << ':' << where.line() << '\n';
  return false;
}

bool run() {
  using pepepow::OfflineStratumSearch;
  using pepepow::SearchStatus;
  pepepow::OfflineStratumController controller;
  if (OfflineStratumSearch::capture(controller, extranonce2) ||
      !prepare(controller, 1) || OfflineStratumSearch::capture(controller, "1234"))
    return failure();
  const auto generation = controller.session().session_generation();
  auto rejected = OfflineStratumSearch::capture(controller, extranonce2);
  if (!rejected || rejected->search(0, 6) != SearchStatus::Exhausted ||
      rejected->make_mock_submit(controller, login)) return failure();
  auto replaced = OfflineStratumSearch::capture(controller, extranonce2);
  if (!replaced || replaced->search(0, 7) != SearchStatus::Found ||
      !fixture_matches(replaced->candidate()) ||
      controller.receive_work_events(notify, generation).applied != 1 ||
      replaced->make_mock_submit(controller, login) ||
      controller.pending_status().submit_pending != 0) return failure();

  auto current = OfflineStratumSearch::capture(controller, extranonce2);
  if (!current || current->search(0, 7) != SearchStatus::Found) return failure();
  // A difficulty update applies to the next notify, preserving captured work.
  constexpr std::string_view new_difficulty =
      R"({"id":null,"method":"mining.set_difficulty","params":[1]})" "\n";
  if (controller.receive_work_events(new_difficulty, generation).applied != 1) return failure();
  const auto request = current->make_mock_submit(controller, login);
  if (request != R"({"id":3,"method":"mining.submit","params":["mock.worker","mock-job","12345678","7169e10b","00000006"]})" "\n" ||
      current->make_mock_submit(controller, login) ||
      controller.pending_status().submit_pending != 1 ||
      controller.receive_work_events(R"({"id":3,"result":true,"error":null})" "\n",
                                     generation).shares_accepted != 1) return failure();
  if (controller.receive_work_events(notify, generation).applied != 1) return failure();
  auto harder = OfflineStratumSearch::capture(controller, extranonce2);
  if (!harder || harder->search(6, 1) != SearchStatus::Exhausted) return failure();
  if (controller.receive_work_events(difficulty, generation).applied != 1 ||
      controller.receive_work_events(notify, generation).applied != 1) return failure();
  auto disconnected = OfflineStratumSearch::capture(controller, extranonce2);
  if (!disconnected || disconnected->search(6, 1) != SearchStatus::Found) return failure();
  controller.disconnected();
  if (!prepare(controller, 4) || disconnected->make_mock_submit(controller, login) ||
      controller.pending_status().submit_pending != 0) return failure();
  auto fresh = OfflineStratumSearch::capture(controller, extranonce2);
  return fresh && fresh->search(6, 1) == SearchStatus::Found &&
         fresh->make_mock_submit(controller, login) ==
             R"({"id":6,"method":"mining.submit","params":["mock.worker","mock-job","12345678","7169e10b","00000006"]})" "\n";
}
} // namespace

int main() {
  if (run()) {
    std::cout << "PASS: native Stratum work, frozen header/digest, mock submit and stale rejection\n";
    return 0;
  }
  std::cerr << "FAIL: native Stratum work or mock submit lifecycle\n";
  return 1;
}
