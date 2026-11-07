#include "pepepow/stratum_notify_decoder.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

// Do not use assert(): CMake Release builds define NDEBUG and would skip tests.
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return EXIT_FAILURE; } } while (false)

int main() {
  const std::string p(64, 'a');
  const std::string base = "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job1\",\"" + p + "\",\"\",\"\",[],\"00004000\",\"1d00ffff\",\"67500000\",true]}";
  auto event = pepepow::decode_notify(base);
  CHECK(event && event->job_id == "job1" && event->clean_jobs);
  pepepow::JobState state;
  CHECK(state.apply({pepepow::JobState::EventKind::Connected}));
  CHECK(state.apply({pepepow::JobState::EventKind::Authorized, true}));
  CHECK(state.apply(*event));
  CHECK(state.active() && state.active()->id == "job1");
  CHECK(!pepepow::decode_notify(base + "junk"));
  auto bad = base;
  bad.replace(bad.find(p), 64, std::string(64, 'z'));
  CHECK(!pepepow::decode_notify(bad));
  bad = base;
  bad.replace(bad.find("true"), 4, "null");
  CHECK(!pepepow::decode_notify(bad));
  bad = base;
  bad.replace(bad.find("job1"), 4, "jo\\b");
  CHECK(!pepepow::decode_notify(bad));
  bad = base;
  bad.replace(bad.find("[]"), 2, "[\"" + p + "\"]");
  CHECK(pepepow::decode_notify(bad));
  bad = base;
  bad.replace(bad.find("true"), 4, "1");
  CHECK(pepepow::decode_notify(bad));
  return EXIT_SUCCESS;
}
