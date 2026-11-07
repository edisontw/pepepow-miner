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
  CHECK(session.subscribe(sub, "1"));
  CHECK(session.jobs().can_submit(*old));
  auto changed = sub;
  changed.replace(changed.find("0102abcd"), 8, "0102abce");
  CHECK(session.subscribe(changed, "1"));
  CHECK(!session.jobs().can_submit(*old) && !session.jobs().active());
  CHECK(session.notify(job));
  auto fresh = session.jobs().active();
  CHECK(fresh && session.jobs().can_submit(*fresh));
  session.disconnected();
  CHECK(!session.jobs().can_submit(*fresh));
  CHECK(!session.notify(job));
  session.connected();
  CHECK(session.subscribe(sub, "1"));
  CHECK(!session.notify(job));
  CHECK(session.authorize(auth, "2"));
  CHECK(session.notify(job));
  CHECK(session.jobs().active() && !session.jobs().can_submit(*fresh));
  return EXIT_SUCCESS;
}
