#include "pepepow/cpu_benchmark.hpp"
#include "pepepow/cpu_worker_pool.hpp"
#include "cpu_benchmark_accounting.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace pepepow {
namespace {
using Clock = SearchClock;

constexpr unsigned default_duration_seconds = 10;
constexpr unsigned max_duration_seconds = 1800;

std::atomic_bool interrupted{false};
static_assert(std::atomic_bool::is_always_lock_free);

void request_stop(int) { interrupted.store(true, std::memory_order_relaxed); }

struct Options {
  unsigned workers = 1;
  unsigned seconds = default_duration_seconds;
};

bool positive_decimal(std::string_view text, unsigned& value) {
  if (text.empty()) return false;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size() && value != 0;
}

void usage(std::ostream& out) {
  out << "Usage: pepepow-cpu-benchmark [--benchmark] [--threads N|all] [--seconds N]\n"
         "       offline_stratum_miner --benchmark [--threads N|all] [--seconds N]\n"
         "Offline pinned-Core HooHash V110 benchmark; no network connections.\n"
         "--threads accepts 1..256 (default 1) or 'all' (detected logical CPUs).\n"
         "--seconds accepts 1..1800 (default 10). Ctrl+C stops cooperatively.\n";
}

std::optional<Options> parse_options(int argc, char** argv, bool& help, std::string& error) {
  Options options;
  bool threads_seen = false;
  bool seconds_seen = false;
  help = false;
  const unsigned logical_cpus = std::thread::hardware_concurrency();
  for (int index = 1; index < argc; ++index) {
    const std::string_view name(argv[index]);
    if (name == "--help" && !help) {
      help = true;
      continue;
    }
    if (name == "--benchmark") continue;
    if (name != "--threads" && name != "--seconds") {
      error = "unknown option '" + std::string(name) + "'";
      return std::nullopt;
    }
    if (index + 1 >= argc) {
      error = "missing value for " + std::string(name);
      return std::nullopt;
    }
    const std::string_view value(argv[++index]);
    if (name == "--threads") {
      if (threads_seen) {
        error = "--threads may be specified only once";
        return std::nullopt;
      }
      threads_seen = true;
      if (value == "all") {
        if (logical_cpus == 0 || logical_cpus > max_cpu_workers) {
          error = "detected logical CPU count is unavailable or exceeds the 256-worker limit";
          return std::nullopt;
        }
        options.workers = logical_cpus;
      } else if (!positive_decimal(value, options.workers) ||
                 options.workers > max_cpu_workers) {
        error = "--threads must be 'all' or an integer in 1..256";
        return std::nullopt;
      }
    } else {
      if (seconds_seen) {
        error = "--seconds may be specified only once";
        return std::nullopt;
      }
      seconds_seen = true;
      if (!positive_decimal(value, options.seconds) || options.seconds > max_duration_seconds) {
        error = "--seconds must be an integer in 1..1800";
        return std::nullopt;
      }
    }
  }
  if (help) return options;
  return options;
}

std::optional<double> process_cpu_seconds() {
#ifdef _WIN32
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    return std::nullopt;
  const auto to_ticks = [](const FILETIME& time) {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
  };
  return static_cast<double>(to_ticks(kernel) + to_ticks(user)) / 10000000.0;
#else
  timespec value{};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0) return std::nullopt;
  return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_nsec) / 1e9;
#endif
}

std::optional<std::uint64_t> current_working_set_bytes() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                            sizeof(counters))) return std::nullopt;
  return static_cast<std::uint64_t>(counters.WorkingSetSize);
#else
  std::ifstream statm("/proc/self/statm");
  std::uint64_t virtual_pages = 0, resident_pages = 0;
  if (!(statm >> virtual_pages >> resident_pages)) return std::nullopt;
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) return std::nullopt;
  return resident_pages * static_cast<std::uint64_t>(page_size);
#endif
}

std::optional<std::uint64_t> peak_working_set_bytes() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                            sizeof(counters))) return std::nullopt;
  return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) return std::nullopt;
#ifdef __APPLE__
  return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
  return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}

const char* os_name() {
#ifdef _WIN32
  return "Windows";
#elif defined(__linux__)
  return "Linux";
#else
  return "unknown";
#endif
}

const char* compiler_name() {
#if defined(__clang__)
  return "Clang " __clang_version__;
#elif defined(__GNUC__)
  return "GCC " __VERSION__;
#elif defined(_MSC_VER)
  return "MSVC";
#else
  return "unknown";
#endif
}

void print_json_string(std::string_view text) {
  std::cout << '"';
  for (const char character : text) {
    if (character == '"' || character == '\\') std::cout << '\\';
    if (character == '\n') std::cout << "\\n";
    else std::cout << character;
  }
  std::cout << '"';
}

void print_optional_number(const std::optional<std::uint64_t>& value) {
  if (value) std::cout << *value;
  else std::cout << "null";
}

void print_hex(const Header& header) {
  constexpr char digits[] = "0123456789abcdef";
  for (const auto byte : header) {
    std::cout << digits[byte >> 4] << digits[byte & 15];
  }
}

int run(const Options& options, SearchClock::time_point process_started) {
  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);
#ifdef SIGBREAK
  std::signal(SIGBREAK, request_stop);
#endif
  interrupted.store(false, std::memory_order_relaxed);

  Header header{};
  for (std::size_t index = 0; index < header.size(); ++index)
    header[index] = static_cast<std::uint8_t>((index * 37 + 11) & 0xff);
  Digest target{};

  CpuWorkerPool pool(options.workers);
  const auto pool_ready = Clock::now();
  const auto startup_ms = std::chrono::duration<double, std::milli>(pool_ready - process_started).count();
  const auto cpu_before = process_cpu_seconds();
  const auto started = Clock::now();
  const auto deadline = started + std::chrono::seconds(options.seconds);
  std::uint64_t total_hashes = 0;
  std::uint64_t next_nonce = 0;
  std::string_view stop_reason = "duration";

  while (!interrupted.load(std::memory_order_relaxed) && Clock::now() < deadline) {
    if (next_nonce > UINT32_MAX) {
      stop_reason = "nonce-space-exhausted";
      break;
    }
    const auto remaining = std::uint64_t{UINT32_MAX} - next_nonce + 1;
    const auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        cpu_benchmark_batch_size, remaining));
    const auto result = pool.search(header, target, static_cast<std::uint32_t>(next_nonce), count,
                                    deadline, &interrupted);
    total_hashes += result.hashes;
    if (result.status == SearchStatus::Exhausted) {
      if (result.hashes != count) {
        std::cerr << "error: worker pool returned an incomplete exhausted range\n";
        return 1;
      }
      next_nonce += count;
      if (next_nonce > UINT32_MAX) {
        stop_reason = "nonce-space-exhausted";
        break;
      }
      continue;
    }
    if (result.status == SearchStatus::Timeout) {
      stop_reason = "duration";
      break;
    }
    if (result.status == SearchStatus::Cancelled) {
      stop_reason = "interrupt";
      break;
    }
    if (result.status == SearchStatus::Found) {
      std::cerr << "error: the fixed benchmark header produced a zero digest; no rate was reported\n";
      return 1;
    }
    std::cerr << "error: worker pool could not start the requested benchmark threads\n";
    return 1;
  }

  if (interrupted.load(std::memory_order_relaxed)) stop_reason = "interrupt";
  const auto stopped = Clock::now();
  const auto elapsed_seconds = std::chrono::duration<double>(stopped - started).count();
  const auto cpu_after = process_cpu_seconds();
  const auto cpu_seconds = cpu_before && cpu_after ?
      std::optional<double>(*cpu_after - *cpu_before) : std::nullopt;
  const unsigned logical_cpus = std::thread::hardware_concurrency();
  const auto cpu_utilization = cpu_seconds && elapsed_seconds > 0 && logical_cpus > 0 ?
      std::optional<double>(*cpu_seconds * 100.0 / elapsed_seconds / logical_cpus) : std::nullopt;
  const auto stop_latency_ms = stop_reason == "duration" && stopped > deadline ?
      std::optional<double>(std::chrono::duration<double, std::milli>(stopped - deadline).count()) :
      std::nullopt;
  const double hashrate = elapsed_seconds > 0 ? total_hashes / elapsed_seconds : 0.0;
  const auto batch_accounting = account_cpu_benchmark_hashes(total_hashes);

  std::cout << std::setprecision(10)
            << "{\"status\":\"ok\",\"mode\":\"offline\",\"os\":";
  print_json_string(os_name());
  std::cout << ",\"compiler\":";
  print_json_string(compiler_name());
  std::cout << ",\"threads\":" << options.workers
            << ",\"logical_cpus\":" << logical_cpus
            << ",\"requested_seconds\":" << options.seconds
            << ",\"elapsed_seconds\":" << elapsed_seconds
            << ",\"hashes\":" << total_hashes
            << ",\"hashrate_hps\":" << hashrate
            << ",\"process_cpu_seconds\":";
  if (cpu_seconds) std::cout << *cpu_seconds;
  else std::cout << "null";
  std::cout << ",\"cpu_utilization_pct_of_logical_capacity\":";
  if (cpu_utilization) std::cout << *cpu_utilization;
  else std::cout << "null";
  std::cout << ",\"memory_current_bytes\":";
  print_optional_number(current_working_set_bytes());
  std::cout << ",\"memory_peak_bytes\":";
  print_optional_number(peak_working_set_bytes());
  std::cout << ",\"startup_ms\":" << startup_ms << ",\"stop_reason\":";
  print_json_string(stop_reason);
  std::cout << ",\"stop_latency_ms\":";
  if (stop_latency_ms) std::cout << *stop_latency_ms;
  else std::cout << "null";
  std::cout << ",\"nonce_batches_complete\":" << batch_accounting.complete_batches
            << ",\"final_batch_hashes\":" << batch_accounting.final_batch_hashes
            << ",\"nonce_batches_disjoint\":true,\"header_hex\":\"";
  print_hex(header);
  std::cout << "\"}\n";
  return 0;
}
}  // namespace

int run_cpu_benchmark(int argc, char** argv, SearchClock::time_point process_started) {
  bool help = false;
  std::string error;
  const auto options = parse_options(argc, argv, help, error);
  if (!options) {
    std::cerr << "error: " << error << '\n';
    usage(std::cerr);
    return 2;
  }
  if (help) {
    usage(std::cout);
    return 0;
  }
  return run(*options, process_started);
}
}  // namespace pepepow
