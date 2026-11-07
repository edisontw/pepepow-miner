#include "pepepow/stratum_notify_decoder.hpp"
#include <cassert>
#include <string>
int main() {
  const std::string p(64,'a');
  const std::string base="{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job1\",\""+p+"\",\"\",\"\",[],\"00004000\",\"1d00ffff\",\"67500000\",true]}";
  auto event=pepepow::decode_notify(base);
  assert(event && event->job_id=="job1" && event->clean_jobs);
  pepepow::JobState state;
  assert(state.apply({pepepow::JobState::EventKind::Connected}));
  assert(state.apply({pepepow::JobState::EventKind::Authorized,true}));
  assert(state.apply(*event));
  assert(state.active()->id=="job1");
  assert(!pepepow::decode_notify(base+"junk"));
  auto bad=base;bad.replace(bad.find(p),64,std::string(64,'z'));assert(!pepepow::decode_notify(bad));
  bad=base;bad.replace(bad.find("true"),4,"null");assert(!pepepow::decode_notify(bad));
  bad=base;bad.replace(bad.find("job1"),4,"jo\\b");assert(!pepepow::decode_notify(bad));
  bad=base;bad.replace(bad.find("[]"),2,"[\""+p+"\"]");assert(pepepow::decode_notify(bad));
  bad=base;bad.replace(bad.find("true"),4,"1");assert(pepepow::decode_notify(bad));
}
