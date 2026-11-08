#include "pepepow/offline_stratum_session.hpp"
#include <cstdlib>
#include <string>
#define CHECK(x) do { if (!(x)) return EXIT_FAILURE; } while (false)
int main() {
  pepepow::OfflineStratumSession session;
  const std::string sub = R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})";
  const std::string auth = R"({"id":2,"result":true,"error":null})";
  const std::string diff = R"({"id":null,"method":"mining.set_difficulty","params":[2]})";
  const std::string prev(64, 'a');
  const std::string job = "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job1\",\"" + prev + "\",\"\",\"\",[],\"00004000\",\"1d00ffff\",\"67500000\",true]}";
  CHECK(!session.subscribe(sub, "1"));
  session.connected();
  const auto first_generation = session.session_generation();
  CHECK(!session.authorize(auth, "2"));
  CHECK(!session.notify(job));
  CHECK(!session.subscribe(sub, "9"));
  CHECK(session.subscribe(sub, "1"));
  CHECK(!session.authorize(auth, "9"));
  CHECK(session.difficulty(diff));
  CHECK(!session.notify(job));
  CHECK(session.authorize(auth, "2"));
  CHECK(session.notify(job));
  auto old = session.jobs().active();
  CHECK(old && old->difficulty == 2.0 && session.jobs().can_submit(*old));
  CHECK(!session.track_submit(std::string(21, '9'), *old));
  CHECK(session.track_submit("3", *old));
  CHECK(!session.track_submit("3", *old));
  CHECK(!session.track_submit("03", *old));
  CHECK(!session.submit_reply(R"({"id":4,"result":true,"error":null})", "3", first_generation));
  CHECK(session.submit_reply(R"({"id":3,"result":true,"error":null})", "3", first_generation) == pepepow::SubmitOutcome::Accepted);
  CHECK(session.share_counters().accepted == 1);
  CHECK(!session.submit_reply(R"({"id":3,"result":true,"error":null})", "3", first_generation));
  CHECK(!session.track_submit("3", *old));
  CHECK(session.track_submit("4", *old));
  CHECK(!session.submit_reply(R"({"id":4,"result":null,"error":[oops]})", "4", first_generation));
  for (unsigned i = 10; i < 4105; ++i) {
    CHECK(session.track_submit(std::to_string(i), *old));
  }
  CHECK(!session.track_submit("4105", *old));
  CHECK(session.subscribe(sub, "1"));
  CHECK(session.jobs().can_submit(*old));
  auto changed = sub;
  changed.replace(changed.find("0102abcd"), 8, "0102abce");
  CHECK(session.subscribe(changed, "1"));
  CHECK(!session.jobs().can_submit(*old) && !session.jobs().active());
  CHECK(session.submit_reply(R"({"id":4,"result":true,"error":null})", "4", first_generation) == pepepow::SubmitOutcome::Accepted);
  CHECK(session.share_counters().accepted == 2);
  CHECK(!session.track_submit("5", *old));
  CHECK(session.notify(job));
  auto fresh = session.jobs().active();
  CHECK(fresh && session.jobs().can_submit(*fresh));
  CHECK(session.track_submit("4105", *fresh));
  CHECK(session.notify(job));
  CHECK(!session.jobs().can_submit(*fresh));
  CHECK(session.submit_reply(R"({"id":4105,"result":false,"error":null})", "4105", first_generation) == pepepow::SubmitOutcome::Rejected);
  fresh = session.jobs().active();
  CHECK(fresh);
  CHECK(session.track_submit("4106", *fresh));
  CHECK(session.submit_reply(R"({"id":4106,"result":false,"error":null})", "4106", first_generation) == pepepow::SubmitOutcome::Rejected);
  CHECK(session.share_counters().rejected == 2);
  CHECK(session.track_submit("4107", *fresh));
  session.disconnected();
  CHECK(!session.submit_reply(R"({"id":7,"result":true,"error":null})", "7", first_generation));
  CHECK(!session.jobs().can_submit(*fresh));
  CHECK(!session.notify(job));
  session.connected();
  CHECK(session.session_generation() != first_generation);
  CHECK(session.share_counters().accepted == 0 && session.share_counters().rejected == 0);
  CHECK(session.subscribe(sub, "1"));
  CHECK(!session.notify(job));
  CHECK(session.authorize(auth, "2"));
  CHECK(session.notify(job));
  CHECK(session.jobs().active() && !session.jobs().can_submit(*fresh));
  auto new_job = session.jobs().active();
  CHECK(new_job && session.track_submit("7", *new_job));
  CHECK(!session.submit_reply(R"({"id":7,"result":true,"error":null})", "7", first_generation));
  CHECK(session.submit_reply(R"({"id":7,"result":true,"error":null})", "7", session.session_generation()) == pepepow::SubmitOutcome::Accepted);
  CHECK(session.share_counters().accepted == 1 && session.share_counters().rejected == 0);
  CHECK(!session.track_submit("7", *new_job));
  CHECK(!session.submit_reply(R"({"id":7,"result":true,"error":null})", "7", session.session_generation()));
  CHECK(session.share_counters().accepted == 1);
  return EXIT_SUCCESS;
}
