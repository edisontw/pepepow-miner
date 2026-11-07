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
