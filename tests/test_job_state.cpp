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
  // A reused job ID and non-clean notification must not revive stale work.
  assert(state.notify("job2", false));
  assert(!state.can_submit(second));
  auto replacement = *state.active();
  assert(state.can_submit(replacement));
  assert(state.notify("job3", false));
  assert(!state.can_submit(replacement));
  auto nonclean = *state.active();
  assert(state.can_submit(nonclean));
  state.invalidate_work();
  assert(!state.active() && !state.can_submit(nonclean));
  assert(state.notify("job-after-reset", false));
  auto after_reset = *state.active();
  assert(state.can_submit(after_reset));
  assert(!state.can_submit(nonclean));
  state.disconnected();
  assert(!state.can_submit(nonclean) && !state.active());
  state.connected();
  state.authorized(true);
  assert(state.notify("job2", true));
  assert(!state.can_submit(second));
  auto third = *state.active();
  assert(third.difficulty == 1.0);  // A new session must not inherit 16.0.
  assert(state.set_difficulty(32.0));
  assert(state.notify("job4", false));
  assert(state.active()->difficulty == 32.0);
  state.authorized(false);
  assert(!state.can_submit(third));
}
