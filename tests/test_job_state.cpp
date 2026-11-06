#include "pepepow/job_state.hpp"
#include <cassert>
#include <limits>
int main() {
  pepepow::JobState state;
  state.connected();
  assert(!state.notify("job1", true));
  state.authorized(true);
  assert(!state.set_difficulty(0.0));
  assert(!state.set_difficulty(std::numeric_limits<double>::infinity()));
  assert(state.set_difficulty(16.0));
  assert(state.notify("job1", true));
  auto first = *state.active();
  assert(first.difficulty == 16.0 && state.can_submit(first));
  assert(state.notify("job2", true));
  assert(!state.can_submit(first));
  auto second = *state.active();
  assert(state.can_submit(second));
  state.disconnected();
  assert(!state.can_submit(second) && !state.active());
  state.connected();
  state.authorized(true);
  assert(state.notify("job2", true));
  assert(!state.can_submit(second));
  auto third = *state.active();
  state.authorized(false);
  assert(!state.can_submit(third));
}
