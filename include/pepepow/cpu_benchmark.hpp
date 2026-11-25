#pragma once

#include "pepepow/bounded_cpu_search.hpp"

namespace pepepow {
int run_cpu_benchmark(int argc, char** argv, SearchClock::time_point process_started);
}
