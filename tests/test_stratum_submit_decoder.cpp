#include "pepepow/stratum_submit_decoder.hpp"
#include <cstdlib>
#define CHECK(x) do { if (!(x)) return EXIT_FAILURE; } while (false)
int main() {
  using pepepow::SubmitOutcome;
  using pepepow::decode_submit_reply;
  CHECK(decode_submit_reply(R"({"id":3,"result":true,"error":null})", "3") == SubmitOutcome::Accepted);
  CHECK(decode_submit_reply(R"({"id":3,"result":false,"error":null})", "3") == SubmitOutcome::Rejected);
  CHECK(decode_submit_reply(R"({"id":3,"result":false,"error":[23,"low difficulty",null]})", "3") == SubmitOutcome::PoolError);
  CHECK(!decode_submit_reply(R"({"id":3,"result":true,"error":[23,"bad",null]})", "3"));
  CHECK(!decode_submit_reply(R"({"id":4,"result":true,"error":null})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":true,"error":null,"extra":1})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":null})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":false,"error":[23,"unfinished]})", "3"));
  CHECK(decode_submit_reply(R"({"id":3,"result":null,"error":{"code":-1,"message":"bad","data":[1,{"nested":true}]}})", "3") == SubmitOutcome::PoolError);
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":[oops]})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":[23,"bad",null,]})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":{"code":01}})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":{"code":1,}})", "3"));
  CHECK(!decode_submit_reply(R"({"id":3,"result":null,"error":[23,"bad\q",null]})", "3"));
  return EXIT_SUCCESS;
}
