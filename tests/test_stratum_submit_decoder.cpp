#include "pepepow/stratum_submit_decoder.hpp"
#include <cstdlib>
#include <string>
#define CHECK(x) do { if (!(x)) return EXIT_FAILURE; } while (false)
int main() {
  using pepepow::SubmitOutcome;
  using pepepow::SubmitErrorClass;
  using pepepow::decode_submit_reply;
  using pepepow::decode_submit_error_class;
  CHECK(decode_submit_reply(R"({"id":3,"result":true,"error":null})", "3") == SubmitOutcome::Accepted);
  CHECK(decode_submit_reply(R"({"id":3,"result":false,"error":null})", "3") == SubmitOutcome::Rejected);
  CHECK(decode_submit_reply(R"({"id":3,"result":false,"error":[23,"low difficulty",null]})", "3") == SubmitOutcome::PoolError);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":[23,"low difficulty",null]})", "3") == SubmitErrorClass::LowDifficulty);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":[23,"duplicate share",null]})", "3") == SubmitErrorClass::Duplicate);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":[23,"stale job",null]})", "3") == SubmitErrorClass::Stale);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":{"code":23,"message":"invalid nonce"}})", "3") == SubmitErrorClass::Invalid);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":null})", "3") == SubmitErrorClass::Rejected);
  CHECK(decode_submit_error_class(R"({"id":3,"result":false,"error":[23,"other",null]})", "3") == SubmitErrorClass::Other);
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
  auto err = [](std::string bytes) {
    return std::string("{\"id\":3,\"result\":null,\"error\":[\"") + bytes + "\"]}";
  };
  CHECK(decode_submit_reply(err("\xe4\xb8\xad"), "3") == SubmitOutcome::PoolError);
  CHECK(decode_submit_reply(err("\xf0\x9f\x98\x80"), "3") == SubmitOutcome::PoolError);
  CHECK(!decode_submit_reply(err("\xc0\x80"), "3"));
  CHECK(!decode_submit_reply(err("\xed\xa0\x80"), "3"));
  CHECK(!decode_submit_reply(err("\xf4\x90\x80\x80"), "3"));
  CHECK(!decode_submit_reply(err("\xe4\xb8"), "3"));
  CHECK(!decode_submit_reply(err("\x80"), "3"));
  return EXIT_SUCCESS;
}
