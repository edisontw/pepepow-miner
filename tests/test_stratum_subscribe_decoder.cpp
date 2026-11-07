#include "pepepow/stratum_subscribe_decoder.hpp"
#include <cstdlib>
#include <string>
#include "pepepow/job_state.hpp"
#define CHECK(x) do { if (!(x)) std::abort(); } while (false)
int main() {
  const std::string valid = R"({"id":1,"result":[[["mining.set_difficulty","abc"],["mining.notify","abc"]],"0102abcd",4],"error":null})";
  auto parsed=pepepow::decode_subscribe_reply(valid,"1");
  CHECK(parsed && parsed->extranonce1=="0102abcd" && parsed->extranonce2_size==4);
  CHECK(!pepepow::decode_subscribe_reply(valid,"2"));
  CHECK(!pepepow::decode_subscribe_reply(valid,"01"));
  auto invalid=[&](std::string from,std::string to) {
    auto changed=valid; auto pos=changed.find(from); CHECK(pos!=std::string::npos);
    changed.replace(pos,from.size(),to);
    CHECK(!pepepow::decode_subscribe_reply(changed,"1"));
  };
  invalid("\"0102abcd\"","\"0102abc\"");
  invalid("\"0102abcd\"","\"0102abcz\"");
  invalid("\"0102abcd\"","\"\"");
  invalid(",4]",",0]");
  invalid(",4]",",33]");
  invalid(",4]",",04]");
  invalid("\"mining.notify\"","\"mining.submit\"");
  invalid("\"error\":null","\"error\":{}");
  invalid("\"id\":1","\"id\":01");
  CHECK(!pepepow::decode_subscribe_reply(valid+"garbage","1"));
  CHECK(!pepepow::decode_subscribe_reply(std::string(65537,'x'),"1"));
  pepepow::JobState jobs;
  pepepow::SubscribeWorkGuard guard;
  jobs.apply({pepepow::JobState::EventKind::Connected});
  jobs.apply({pepepow::JobState::EventKind::Authorized, true});
  CHECK(guard.accept(*parsed, jobs));
  CHECK(jobs.apply({pepepow::JobState::EventKind::Notify, false, 0.0, "job1"}));
  auto old = jobs.active();
  CHECK(old && jobs.can_submit(*old));
  CHECK(guard.accept(*parsed, jobs)); // identical response preserves work
  CHECK(jobs.can_submit(*old));
  auto changed = *parsed;
  changed.extranonce1 = "0102abce";
  CHECK(guard.accept(changed, jobs));
  CHECK(!jobs.active() && !jobs.can_submit(*old));
  CHECK(jobs.apply({pepepow::JobState::EventKind::Notify, false, 0.0, "job2"}));
  auto next = jobs.active();
  CHECK(next && jobs.can_submit(*next));
  changed.extranonce2_size = 8;
  CHECK(guard.accept(changed, jobs));
  CHECK(!jobs.can_submit(*next));
  jobs.apply({pepepow::JobState::EventKind::Disconnected});
  guard.reset();
  CHECK(guard.accept(*parsed, jobs)); // first response in new session
  CHECK(!jobs.active());
}
