#include "pepepow/stratum_difficulty_decoder.hpp"
#include <cassert>
#include <string_view>
int main() {
  using pepepow::decode_difficulty;
  auto valid = decode_difficulty(R"({"id":null,"method":"mining.set_difficulty","params":[16.384]})");
  assert(valid && valid->difficulty == 16.384);
  pepepow::JobState state;
  assert(state.apply({pepepow::JobState::EventKind::Connected}));
  assert(state.apply(*valid));
  assert(state.apply({pepepow::JobState::EventKind::Authorized, true}));
  assert(state.notify("test", true) && state.active()->difficulty == 16.384);

  using pepepow::decode_authorize_reply;
  auto auth = decode_authorize_reply(R"({"id":2,"result":true,"error":null})", "2");
  assert(auth && auth->kind == pepepow::JobState::EventKind::Authorized && auth->authorized);
  pepepow::JobState auth_state;
  assert(auth_state.apply({pepepow::JobState::EventKind::Connected}));
  assert(auth_state.apply(*auth));
  assert(auth_state.notify("authorized-job", true));
  auto denied = decode_authorize_reply(R"({"id":2,"result":false,"error":null})", "2");
  assert(denied && !denied->authorized);
  assert(auth_state.apply(*denied));
  assert(!auth_state.active());
  // Even if the caller supplies the same text, 02 is not a legal JSON number.
  assert(!decode_authorize_reply(R"({"id":02,"result":true,"error":null})", "02"));
  for (std::string_view bad : {
      R"({"id":3,"result":true,"error":null})",
      R"({"id":2,"result":true,"error":[20,"denied",null]})",
      R"({"id":2,"result":1,"error":null})",
      R"({"id":2,"result":true,"error":null,"extra":1})",
      R"({"id":2,"result":true,"error":null,"result":false})",
      R"({"id":"2","result":true,"error":null})",
      R"({"id":02,"result":true,"error":null})"}) {
    assert(!decode_authorize_reply(bad, "2"));
  }
  for (std::string_view bad : {
      R"({"id":null,"method":"mining.set_difficulty","params":[0]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[-1]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[NaN]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[1e999]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[01]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[1,2]})",
      R"({"id":null,"method":"mining.notify","params":[16]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[16],"extra":1})",
      R"({"id":null,"method":"mining.set_difficulty","params":[16],"params":[2]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[1.]})",
      R"({"id":null,"method":"mining.set_difficulty","params":[1e]})"}) {
    assert(!decode_difficulty(bad));
  }
}
