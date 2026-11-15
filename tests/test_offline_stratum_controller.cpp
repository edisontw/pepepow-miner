#include "pepepow/offline_stratum_controller.hpp"
#include <cstdlib>
#include <string>
#include <vector>
#define CHECK(x) do { if (!(x)) return EXIT_FAILURE; } while (false)
int main() {
  pepepow::OfflineStratumController c;
  const std::string sub = R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})";
  const std::string auth = R"({"id":2,"result":true,"error":null})";
  const std::string diff = R"({"id":null,"method":"mining.set_difficulty","params":[2]})";
  const std::string prev(64, 'a');
  const std::string job = "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job1\",\"" + prev + "\",\"\",\"\",[],\"00004000\",\"1d00ffff\",\"67500000\",true]}";
  CHECK(!c.begin_subscribe());
  c.connected(); auto g = c.session().session_generation();
  CHECK(!c.begin_authorize());
  CHECK(c.begin_subscribe() == 1); CHECK(!c.begin_subscribe());
  CHECK(!c.subscribe_reply(sub, g + 1)); CHECK(c.subscribe_reply(sub, g));
  CHECK(!c.subscribe_reply(sub, g));
  CHECK(c.begin_authorize() == 2); CHECK(!c.authorize_reply(auth, g + 1));
  CHECK(c.authorize_reply(auth, g)); CHECK(c.difficulty(diff)); CHECK(c.notify(job));
  auto work = c.session().jobs().active(); CHECK(work);
  CHECK(c.begin_submit(*work) == 3);
  CHECK(!c.submit_reply(R"({"id":3,"result":true,"error":null})", 3, g + 1));
  CHECK(c.submit_reply(R"({"id":3,"result":true,"error":null})", 3, g) == pepepow::SubmitOutcome::Accepted);
  CHECK(c.begin_submit(*work) == 4);
  CHECK(!c.cancel_request(4, g + 1));
  CHECK(c.cancel_request(4, g));
  CHECK(!c.cancel_request(4, g));
  CHECK(!c.submit_reply(R"({"id":4,"result":true,"error":null})", 4, g));
  CHECK(c.session().share_counters().accepted == 1);
  CHECK(c.begin_subscribe() == 5);
  CHECK(c.cancel_request(5, g));
  CHECK(!c.subscribe_reply(R"({"id":5,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})", g));
  CHECK(c.begin_authorize() == 6);
  CHECK(c.cancel_request(6, g));
  CHECK(!c.authorize_reply(R"({"id":6,"result":true,"error":null})", g));
  c.disconnected(); c.connected();
  CHECK(!c.begin_authorize()); CHECK(c.begin_subscribe() == 7);
  CHECK(!c.subscribe_reply(sub, g));
  const std::string sub_reconnected = R"({"id":7,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})";
  CHECK(!c.subscribe_reply(sub, c.session().session_generation()));
  CHECK(c.subscribe_reply(sub_reconnected, c.session().session_generation()));
  CHECK(c.begin_authorize() == 8);
  CHECK(!c.cancel_request(8, g));
  // Atomic request encoding: invalid data must not reserve or consume IDs.
  pepepow::OfflineStratumController wire;
  wire.connected();
  const auto wg = wire.session().session_generation();
  CHECK(!wire.make_authorize_request("wallet"));
  CHECK(wire.make_subscribe_request() ==
        "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[]}\n");
  CHECK(!wire.make_subscribe_request());
  CHECK(wire.subscribe_reply(sub, wg));
  CHECK(!wire.make_authorize_request("bad\nlogin"));
  CHECK(wire.make_authorize_request("WALLET.worker") ==
        "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"WALLET.worker\",\"x\"]}\n");
  CHECK(wire.authorize_reply(auth, wg));
  CHECK(wire.notify(job));
  auto active = wire.session().jobs().active(); CHECK(active);
  CHECK(!wire.make_submit_request(*active, "WALLET.worker", "xx", 1, "67500000", "12345678"));
  CHECK(wire.make_submit_request(*active, "WALLET.worker", "00", 1, "67500000", "12345678") ==
        "{\"id\":3,\"method\":\"mining.submit\",\"params\":[\"WALLET.worker\",\"job1\",\"00\",\"67500000\",\"12345678\"]}\n");
  CHECK(wire.cancel_request(3, wg));
  CHECK(!wire.submit_reply(R"({"id":3,"result":true,"error":null})", 3, wg));
  CHECK(wire.make_submit_request(*active, "WALLET.worker", "01", 1, "67500000", "12345678").has_value());
  wire.disconnected(); wire.connected();
  CHECK(!wire.begin_submit(*active)); // stale job must not consume the next ID
  CHECK(!wire.make_submit_request(*active, "WALLET.worker", "01", 1, "67500000", "12345678"));
  CHECK(!wire.cancel_request(4, wg));
  // Fragmented transport data must not leak across connection generations.
  CHECK(wire.receive_bytes("partial", wg).lines.empty());
  CHECK(wire.receive_bytes("ignored\n", wg).lines.empty());
  const auto new_generation = wire.session().session_generation();
  CHECK(wire.receive_bytes("first\nsecond", new_generation).lines ==
        std::vector<std::string>({"first"}));
  CHECK(wire.receive_bytes(" line\n", new_generation).lines ==
        std::vector<std::string>({"second line"}));
  wire.disconnected();
  CHECK(wire.receive_bytes("ignored\n", new_generation).lines.empty());
  wire.connected();
  CHECK(wire.receive_bytes("fresh\n", wire.session().session_generation()).lines ==
        std::vector<std::string>({"fresh"}));
  CHECK(wire.make_subscribe_request().has_value());
  // Bytes arriving in fragments now reach the work-state decoders.
  pepepow::OfflineStratumController events;
  events.connected();
  auto eg = events.session().session_generation();
  CHECK(events.make_subscribe_request().has_value());
  CHECK(events.subscribe_reply(sub, eg));
  CHECK(events.make_authorize_request("WALLET.worker").has_value());
  CHECK(events.authorize_reply(auth, eg));
  // A single framed stream handles fragmented subscribe/authorize replies.
  pepepow::OfflineStratumController handshake;
  handshake.connected();
  auto hg = handshake.session().session_generation();
  CHECK(handshake.make_subscribe_request().has_value());
  CHECK(handshake.receive_work_events(sub.substr(0, 12), hg).subscribed == 0);
  auto hs = handshake.receive_work_events(sub.substr(12) + "\n", hg);
  CHECK(hs.subscribed == 1 && hs.ignored == 0);
  CHECK(handshake.make_authorize_request("WALLET.worker").has_value());
  auto ha = handshake.receive_work_events(auth + "\n", hg);
  CHECK(ha.authorized == 1 && ha.ignored == 0);
  auto hw = handshake.receive_work_events(diff + "\n" + job + "\n", hg);
  CHECK(hw.applied == 2 && handshake.session().jobs().active());
  CHECK(handshake.receive_work_events(sub + "\n", hg).ignored == 1);
  // Framed submit replies must match a pending ID and current generation.
  auto current = handshake.session().jobs().active(); CHECK(current);
  CHECK(handshake.begin_submit(*current) == 3);
  auto accepted = handshake.receive_work_events(
      R"({"id":3,"result":true,"error":null})" "\n", hg);
  CHECK(accepted.shares_accepted == 1 && accepted.ignored == 0);
  CHECK(handshake.session().share_counters().accepted == 1);
  CHECK(handshake.receive_work_events(
      R"({"id":3,"result":true,"error":null})" "\n", hg).ignored == 1);
  CHECK(handshake.begin_submit(*current) == 4);
  CHECK(handshake.receive_work_events(
      R"({"id":5,"result":true,"error":null})" "\n", hg).ignored == 1);
  CHECK(handshake.receive_work_events(
      R"({"id":4,"result":true,"error":null,"extra":1})" "\n", hg).ignored == 1);
  CHECK(handshake.receive_work_events(
      R"({"id":4,"result":false,"error":null})" "\n", hg).shares_rejected == 1);
  CHECK(handshake.begin_submit(*current) == 5);
  CHECK(handshake.receive_work_events(
      R"({"id":5,"result":null,"error":[23,"low difficulty",null]})" "\n", hg).shares_pool_errors == 1);
  CHECK(handshake.begin_submit(*current) == 6);
  CHECK(handshake.receive_work_events(
      R"({"id":18446744073709551616,"result":true,"error":null})" "\n", hg).ignored == 1);
  CHECK(handshake.receive_work_events(
      R"({"id":6,"result":true,"error":null})" "\n", hg + 1).shares_accepted == 0);
  CHECK(handshake.receive_work_events(
      R"({"id":6,"result":true,"error":null})" "\n", hg).shares_accepted == 1);
  handshake.disconnected();
  handshake.connected();
  CHECK(handshake.receive_work_events(auth + "\n", hg).authorized == 0);
  CHECK(events.receive_work_events(diff.substr(0, 9), eg).applied == 0);
  auto difficulty_events = events.receive_work_events(diff.substr(9) + "\n", eg);
  CHECK(difficulty_events.applied == 1 && difficulty_events.ignored == 0);
  auto notify_events = events.receive_work_events(job + "\n", eg);
  CHECK(notify_events.applied == 1 && events.session().jobs().active());
  CHECK(events.receive_work_events("not json\n", eg).ignored == 1);
  events.disconnected();
  events.connected();
  CHECK(events.receive_work_events(diff + "\n", eg).applied == 0);
  // Saturation must not burn an ID; freeing one slot resumes at the next ID.
  pepepow::OfflineStratumController bounded;
  bounded.connected();
  const auto bg = bounded.session().session_generation();
  CHECK(bounded.begin_subscribe() == 1);
  CHECK(bounded.subscribe_reply(sub, bg));
  CHECK(bounded.begin_authorize() == 2);
  CHECK(bounded.authorize_reply(auth, bg));
  CHECK(bounded.notify(job));
  auto bw = bounded.session().jobs().active(); CHECK(bw);
  for (std::uint64_t id = 3; id < 4099; ++id)
    CHECK(bounded.begin_submit(*bw) == id);
  CHECK(!bounded.begin_submit(*bw));
  CHECK(bounded.cancel_request(3, bg));
  CHECK(bounded.begin_submit(*bw) == 4099);
  // Deterministic monotonic timeout: no sleep or wall clock required.
  pepepow::OfflineStratumController timed;
  timed.connected();
  const auto tg = timed.session().session_generation();
  CHECK(timed.make_subscribe_request().has_value());
  CHECK(!timed.expire_request(1, tg, 100, 109, 10));
  CHECK(!timed.expire_request(1, tg, 100, 99, 10));
  CHECK(!timed.expire_request(1, tg, 100, 110, 0));
  CHECK(!timed.expire_request(1, tg + 1, 100, 110, 10));
  CHECK(timed.pending_status().subscribe_pending);
  CHECK(timed.expire_request(1, tg, 100, 110, 10));
  CHECK(!timed.pending_status().subscribe_pending);
  CHECK(!timed.expire_request(1, tg, 100, 200, 10));
  CHECK(!timed.subscribe_reply(sub, tg));
  CHECK(timed.make_subscribe_request().has_value()); // ID 2, never reused
  timed.disconnected(); timed.connected();
  CHECK(!timed.expire_request(2, tg, 100, 200, 10));
  CHECK(timed.make_subscribe_request().has_value()); // ID 3
  const auto tg2 = timed.session().session_generation();
  CHECK(timed.subscribe_reply(R"({"id":3,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})", tg2));
  CHECK(timed.make_authorize_request("WALLET.worker").has_value()); // ID 4
  CHECK(!timed.expire_request(4, tg2, 500, 509, 10));
  CHECK(timed.expire_request(4, tg2, 500, 510, 10));
  CHECK(!timed.authorize_reply(R"({"id":4,"result":true,"error":null})", tg2));
  CHECK(timed.make_authorize_request("WALLET.worker").has_value()); // ID 5
  return EXIT_SUCCESS;
}
