#include "pepepow/cpu_benchmark.hpp"

int main(int argc, char** argv) {
  const auto process_started = pepepow::SearchClock::now();
  return pepepow::run_cpu_benchmark(argc, argv, process_started);
}
