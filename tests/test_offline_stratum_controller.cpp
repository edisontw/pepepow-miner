#include "pepepow/offline_stratum_controller.hpp"
#include <cstdlib>
#include <string>
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
  CHECK(!c.begin_authorize()); CHECK(c.begin_subscribe() == 1);
  CHECK(!c.subscribe_reply(sub, g));
  CHECK(c.subscribe_reply(sub, c.session().session_generation()));
  CHECK(c.begin_authorize() == 2);
  CHECK(!c.cancel_request(2, g));
  return EXIT_SUCCESS;
}
