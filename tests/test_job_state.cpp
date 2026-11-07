#include "pepepow/job_state.hpp"
#include <cassert>
#include <limits>
int main() {
  pepepow::JobState state;
  // A stale authorize response must not revive a disconnected session.
  state.authorized(true);
  assert(!state.notify("offline", true));
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
  state.authorized(true);
  assert(!state.notify("late-auth", true));
  state.connected();
  state.authorized(true);
  assert(state.notify("job2", true));
  assert(!state.can_submit(second));
  auto third = *state.active();
  assert(third.difficulty == 1.0);  // A new session must not inherit 16.0.
  assert(state.set_difficulty(32.0));
  assert(state.notify("job4", false));
  assert(state.active()->difficulty == 32.0);
  auto current = *state.active();
  assert(state.can_submit(current));
  // A candidate must retain the difficulty/target bound to its job.
  auto tampered = current;
  tampered.difficulty = 1.0;
  assert(!state.can_submit(tampered));
  // A later set_difficulty applies to a future job, not an in-flight job.
  assert(state.set_difficulty(64.0));
  assert(state.can_submit(current));
  assert(state.notify("job5", false));
  assert(!state.can_submit(current));
  assert(state.active()->difficulty == 64.0);
  state.authorized(false);
  assert(!state.can_submit(third));
  // Protocol-decoded events must use the same lifecycle rules as direct calls.
  using Kind = pepepow::JobState::EventKind;
  using Event = pepepow::JobState::Event;
  pepepow::JobState wired;
  assert(wired.apply(Event{Kind::Connected}));
  assert(!wired.apply(Event{Kind::Difficulty, false, 8.0}));
  assert(!wired.apply(Event{Kind::Notify, false, 0.0, "early", true}));
  assert(wired.apply(Event{Kind::Authorized, true}));
  assert(!wired.apply(Event{Kind::Difficulty, false, 0.0}));
  assert(wired.apply(Event{Kind::Difficulty, false, 8.0}));
  assert(wired.apply(Event{Kind::Notify, false, 0.0, "event-job", true}));
  auto queued = *wired.active();
  assert(queued.difficulty == 8.0 && wired.can_submit(queued));
  assert(wired.apply(Event{Kind::ExtranonceChanged}));
  assert(!wired.can_submit(queued) && !wired.active());
  assert(wired.apply(Event{Kind::Notify, false, 0.0, "event-job", false}));
  auto renewed = *wired.active();
  assert(wired.can_submit(renewed) && !wired.can_submit(queued));
  assert(wired.apply(Event{Kind::Disconnected}));
  assert(!wired.can_submit(renewed));
  assert(!wired.apply(Event{Kind::ExtranonceChanged}));
  assert(wired.apply(Event{Kind::Connected}));
  assert(wired.apply(Event{Kind::Authorized, true}));
  assert(wired.apply(Event{Kind::Notify, false, 0.0, "new-session", true}));
  assert(wired.active()->difficulty == 1.0);

}
