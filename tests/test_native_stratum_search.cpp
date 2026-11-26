#include "pepepow/offline_stratum_search.hpp"
#include "pepepow/cpu_worker_pool.hpp"

#include <barrier>
#include <cfenv>

#include <atomic>
#include <iostream>
#include <source_location>

namespace {
constexpr std::string_view login = "mock.worker";
constexpr std::string_view extranonce2 = "12345678";
constexpr std::string_view notify =
    R"({"id":null,"method":"mining.notify","params":["mock-job","000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f","0102","0304",["202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"],"20004000","1d02e371","7169e10b",false]})" "\n";
constexpr std::string_view difficulty =
    R"({"id":null,"method":"mining.set_difficulty","params":[3.2768]})" "\n";

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

bool workers_match() {
  using namespace pepepow;
  constexpr auto first = UINT32_MAX - 31;
  constexpr std::uint32_t count = 32;
  Header header{};
  for (std::size_t i = 0; i < header.size(); ++i) header[i] = static_cast<std::uint8_t>(7 * i);
  std::array<Digest, count> expected{};
  for (std::uint32_t i = 0; i < count; ++i) {
    pepepow_header_set_nonce(header.data(), header.size(), first + i);
    hoohashv110(header.data(), header.size(), expected[i].data());
  }
  const int rounding = std::fegetround();
  for (const unsigned workers : {1u, 2u, 4u, 8u, 12u}) {
    std::atomic_bool mismatch = false;
    std::barrier start_line(static_cast<std::ptrdiff_t>(workers));
    std::vector<std::jthread> threads;
    std::uint64_t next = first;
    for (unsigned index = 0; index < workers; ++index) {
      const auto range = partition_nonce_range(first, count, index, workers);
      if (range.start != next || !range.count) mismatch = true;
      next += range.count;
      threads.emplace_back([&, range, local = header]() mutable {
        start_line.arrive_and_wait();
        for (std::uint32_t offset = 0; offset < range.count; ++offset) {
          const auto nonce = range.start + offset;
          pepepow_header_set_nonce(local.data(), local.size(), nonce);
          Digest actual{};
          hoohashv110(local.data(), local.size(), actual.data());
          if (actual != expected[nonce - first] || std::fegetround() != rounding) mismatch = true;
        }
      });
    }
    for (auto& thread : threads) thread.join();
    if (mismatch || next != std::uint64_t{UINT32_MAX} + 1) return failure();
    CpuWorkerPool pool(workers);
    const auto deadline = SearchClock::now() + std::chrono::seconds(5);
    const auto rejected = pool.search(header, Digest{}, first, count, deadline);
    if (rejected.status != SearchStatus::Exhausted || rejected.hashes != count) return failure();
    const auto exact = pool.search(header, expected.back(), UINT32_MAX, 1, deadline);
    if (!exact.candidate || exact.candidate->nonce != UINT32_MAX ||
        exact.candidate->digest != expected.back() || exact.hashes != 1) return failure();
    if (pool.search(header, Digest{}, UINT32_MAX, 2, deadline).status !=
        SearchStatus::Invalid) return failure();
    Digest easy;
    easy.fill(0xff);
    const auto found = pool.search(header, easy, first, count, deadline);
    if (!found.candidate || found.candidate->nonce != first ||
        found.candidate->digest != expected.front()) return failure();
  }
  return true;
}

int benchmark() {
  using namespace pepepow;
  Header header{};
  constexpr unsigned repetitions = 3;
  constexpr unsigned count = 10000;
  for (const unsigned workers : {1u, 2u, 4u, 8u}) {
    CpuWorkerPool pool(workers);
    const auto started = SearchClock::now();
    for (unsigned repeat = 0; repeat < repetitions; ++repeat) {
      const auto result = pool.search(header, Digest{}, repeat * count, count,
                                      started + std::chrono::seconds(20));
      if (result.status != SearchStatus::Exhausted || result.hashes != count) return 1;
    }
    const auto seconds = std::chrono::duration<double>(SearchClock::now() - started).count();
    std::cout << workers << ' ' << repetitions * count << ' ' << seconds << ' '
              << repetitions * count / seconds << '\n';
  }
  return 0;
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
  auto cancelled = OfflineStratumSearch::capture(controller, extranonce2);
  std::atomic_bool cancel{true};
  if (!cancelled || cancelled->search(0, 7, 1000, &cancel) != SearchStatus::Cancelled ||
      cancelled->candidate()) return failure();
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
      R"({"id":null,"method":"mining.set_difficulty","params":[65536]})" "\n";
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

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--benchmark") return benchmark();
  if (run() && workers_match()) {
    std::cout << "PASS: native Stratum work, frozen header/digest, mock submit and stale rejection\n";
    return 0;
  }
  std::cerr << "FAIL: native Stratum work or mock submit lifecycle\n";
  return 1;
}
