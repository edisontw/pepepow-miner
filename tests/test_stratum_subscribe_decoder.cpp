#include "pepepow/stratum_subscribe_decoder.hpp"
#include <cstdlib>
#include <string>
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
}
