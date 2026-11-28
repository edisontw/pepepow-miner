#include "pepepow/local_handshake.hpp"
#include "pepepow/cpu_benchmark.hpp"
#include "pepepow/cpu_worker_pool.hpp"
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
#include "pepepow/cuda_search_process.hpp"
#endif
#include "pepepow/offline_stratum_search.hpp"
#include "pepepow/stratum_nonce_cursor.hpp"
#include "pepepow/stratum_endpoint.hpp"

#include <atomic>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>

namespace {
using pepepow::OfflineStratumController;
using pepepow::SearchStatus;
using pepepow::socket_io::Socket;
using pepepow::socket_io::invalid_socket;
using pepepow::socket_io::Clock;
using pepepow::socket_io::Deadline;
using pepepow::socket_io::Readiness;
using pepepow::socket_io::socket_ready;

std::atomic_bool stop_requested{false};
static_assert(std::atomic_bool::is_always_lock_free);
void request_stop(int) { stop_requested.store(true, std::memory_order_relaxed); }

constexpr auto request_budget = std::chrono::seconds(2);

struct Options {
  unsigned port = 0;
  unsigned fallback_port = 0;
  unsigned retries = 0;
  unsigned workers = 1;
  unsigned backoff_ms = 100;
  std::string_view mode = "POOL";
  std::string login;
  std::string extranonce2;
  std::uint32_t start = 0;
  std::uint32_t count = 0;
  std::uint32_t timeout_ms = pepepow::default_search_timeout_ms;
  std::uint32_t response_goal = 1;
  std::uint32_t cuda_batch_size = 128;
  double wire_scale = pepepow::OfflineStratumSearch::default_wire_difficulty_scale;
  bool live_pool = false;
  bool cuda_device = false;
  std::uint32_t max_runtime_seconds = 0;
  std::uint32_t submit_interval_ms = 250;
  std::filesystem::path cuda_engine;
  std::filesystem::path cuda_cpu_oracle;
  std::optional<pepepow::socket_io::StratumTcpEndpoint> endpoint;
  std::optional<pepepow::socket_io::StratumTcpEndpoint> fallback_endpoint;
};

template <typename Integer>
bool decimal(std::string_view text, Integer& value) {
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
std::filesystem::path path_from_cli(std::string_view value) {
  std::u8string utf8;
  utf8.reserve(value.size());
  for (const unsigned char byte : value) utf8.push_back(static_cast<char8_t>(byte));
  return std::filesystem::path(utf8);
}
#endif

bool parse_options(int argc, char** argv, Options& options) {
  if (argc < 6) return false;
  if (!decimal(std::string_view(argv[1]), options.port) ||
      options.port == 0 || options.port > 65535) return false;
  options.login = argv[2];
  if (!pepepow::encode_authorize_request(1, options.login)) return false;
  if (!decimal(std::string_view(argv[3]), options.start) ||
      !decimal(std::string_view(argv[4]), options.count) ||
      !pepepow::valid_search_range(options.start, options.count)) return false;
  options.extranonce2 = argv[5];
  bool saw_timeout = false;
  bool saw_responses = false;
  bool saw_scale = false;
  bool saw_retries = false;
  bool saw_backoff = false;
  bool saw_mode = false;
  bool saw_workers = false;

  for (int i = 6; i < argc; ++i) {
    const std::string_view name(argv[i]);
    if (i + 1 >= argc) return false;
    const std::string_view value(argv[++i]);
    if (name == "--max-ms") {
      if (saw_timeout || !decimal(value, options.timeout_ms) || options.timeout_ms == 0 ||
          options.timeout_ms > pepepow::max_search_timeout_ms) return false;
      saw_timeout = true;
    } else if (name == "--responses") {
      if (saw_responses || !decimal(value, options.response_goal) || options.response_goal == 0 ||
          options.response_goal > 4096) return false;
      saw_responses = true;
    } else if (name == "--threads") {
      if (saw_workers || !decimal(value, options.workers) || !options.workers ||
          options.workers > pepepow::max_cpu_workers) return false;
      saw_workers = true;
    } else if (name == "--retries") {
      if (saw_retries || !decimal(value, options.retries) || options.retries > 10) return false;
      saw_retries = true;
    } else if (name == "--fallback-port") {
      if (options.fallback_port || !decimal(value, options.fallback_port) ||
          !options.fallback_port || options.fallback_port > 65535 ||
          options.fallback_port == options.port) return false;
    } else if (name == "--backoff-ms") {
      if (saw_backoff || !decimal(value, options.backoff_ms) ||
          !options.backoff_ms || options.backoff_ms > 2000) return false;
      saw_backoff = true;
    } else if (name == "--mode") {
      if (saw_mode || (value != "POOL" && value != "SOLO")) return false;
      options.mode = value;
      saw_mode = true;
    } else if (name == "--scale") {
      const auto [end, error] =
          std::from_chars(value.data(), value.data() + value.size(), options.wire_scale);
      if (saw_scale || error != std::errc{} || end != value.data() + value.size() ||
          !std::isfinite(options.wire_scale) || options.wire_scale < 1.0) return false;
      saw_scale = true;
    } else {
      return false;
    }
  }
  return !options.fallback_port || options.retries != 0;
}

#ifdef PEPEPOW_ENABLE_LIVE_POOL_MINER
bool parse_live_options(int argc, char** argv, Options& options) {
  bool saw_endpoint = false;
  bool saw_login = false;
  bool saw_extranonce = false;
  bool saw_threads = false;
  bool saw_responses = false;
  bool saw_runtime = false;
  bool saw_mode = false;
  bool saw_batch = false;
  bool saw_submit_interval = false;
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
  bool saw_device = false;
  bool saw_cuda_engine = false;
  bool saw_cuda_oracle = false;
  bool saw_cuda_batch = false;
#endif
  bool saw_retries = false;
  bool saw_scale = false;
  options.live_pool = true;
  options.count = 10000;
  options.backoff_ms = 100;
  for (int i = 2; i < argc; ++i) {
    const std::string_view name(argv[i]);
    if (i + 1 >= argc) return false;
    const std::string_view value(argv[++i]);
    if (name == "--endpoint") {
      if (saw_endpoint) return false;
      pepepow::socket_io::StratumTcpEndpoint endpoint;
      if (!pepepow::socket_io::parse_stratum_tcp_endpoint(value, endpoint)) return false;
      options.endpoint = std::move(endpoint);
      saw_endpoint = true;
    } else if (name == "--fallback-endpoint") {
      if (options.fallback_endpoint) return false;
      pepepow::socket_io::StratumTcpEndpoint endpoint;
      if (!pepepow::socket_io::parse_stratum_tcp_endpoint(value, endpoint)) return false;
      options.fallback_endpoint = std::move(endpoint);
    } else if (name == "--login") {
      if (saw_login || !pepepow::encode_authorize_request(1, value)) return false;
      options.login = value;
      saw_login = true;
    } else if (name == "--extranonce2") {
      if (saw_extranonce || value.empty() || value.size() > 64 || value.size() % 2 != 0)
        return false;
      for (const char ch : value) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
              (ch >= 'A' && ch <= 'F'))) return false;
      }
      options.extranonce2 = value;
      saw_extranonce = true;
    } else if (name == "--mode") {
      if (saw_mode || (value != "POOL" && value != "SOLO")) return false;
      options.mode = value;
      saw_mode = true;
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
    } else if (name == "--device") {
      if (saw_device || (value != "cpu" && value != "cuda")) return false;
      options.cuda_device = value == "cuda";
      saw_device = true;
    } else if (name == "--cuda-engine") {
      if (saw_cuda_engine || value.empty()) return false;
      options.cuda_engine = path_from_cli(value);
      saw_cuda_engine = true;
    } else if (name == "--cpu-oracle") {
      if (saw_cuda_oracle || value.empty()) return false;
      options.cuda_cpu_oracle = path_from_cli(value);
      saw_cuda_oracle = true;
    } else if (name == "--cuda-batch-size") {
      if (saw_cuda_batch || !decimal(value, options.cuda_batch_size) ||
          options.cuda_batch_size == 0 || options.cuda_batch_size > 256) return false;
      saw_cuda_batch = true;
#endif
    } else if (name == "--threads") {
      if (saw_threads || !decimal(value, options.workers) || !options.workers ||
          options.workers > pepepow::max_cpu_workers) return false;
      saw_threads = true;
    } else if (name == "--responses") {
      if (saw_responses || !decimal(value, options.response_goal) ||
          options.response_goal == 0 || options.response_goal > 10000) return false;
      saw_responses = true;
    } else if (name == "--max-runtime-seconds") {
      if (saw_runtime || !decimal(value, options.max_runtime_seconds) ||
          options.max_runtime_seconds < 10 || options.max_runtime_seconds > 1800) return false;
      saw_runtime = true;
    } else if (name == "--batch-size") {
      if (saw_batch || !decimal(value, options.count) || options.count == 0 ||
          options.count > 10000000) return false;
      saw_batch = true;
    } else if (name == "--submit-interval-ms") {
      if (saw_submit_interval || !decimal(value, options.submit_interval_ms) ||
          options.submit_interval_ms < 100 || options.submit_interval_ms > 10000) return false;
      saw_submit_interval = true;
    } else if (name == "--retries") {
      if (saw_retries || !decimal(value, options.retries) || options.retries > 10) return false;
      saw_retries = true;
    } else if (name == "--scale") {
      const auto [end, error] =
          std::from_chars(value.data(), value.data() + value.size(), options.wire_scale);
      if (saw_scale || error != std::errc{} || end != value.data() + value.size() ||
          !std::isfinite(options.wire_scale) || options.wire_scale < 1.0) return false;
      saw_scale = true;
    } else {
      return false;
    }
  }
  if (!saw_endpoint || !saw_login || !saw_extranonce || !saw_threads ||
      !saw_responses || !saw_runtime || !saw_mode ||
      (options.fallback_endpoint && options.retries == 0)) return false;
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
  if (options.cuda_device != (saw_cuda_engine && saw_cuda_oracle) ||
      saw_cuda_engine != saw_cuda_oracle) return false;
  if (options.cuda_device) {
    std::error_code engine_error;
    std::error_code oracle_error;
    if (!std::filesystem::is_regular_file(options.cuda_engine, engine_error) ||
        !std::filesystem::is_regular_file(options.cuda_cpu_oracle, oracle_error))
      return false;
  }
#endif
  if (options.fallback_endpoint && options.endpoint &&
      options.fallback_endpoint->host == options.endpoint->host &&
      options.fallback_endpoint->port == options.endpoint->port) return false;
  return true;
}
#endif

void event(std::string_view name, std::string_view fields = {}) {
  std::cout << "{\"event\":\"" << name << "\"";
  if (!fields.empty()) std::cout << ',' << fields;
  std::cout << "}\n" << std::flush;
}

std::string json_escape(std::string_view value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string escaped;
  escaped.reserve(value.size());
  for (const unsigned char character : value) {
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (character < 0x20U) {
          escaped += "\\u00";
          escaped.push_back(digits[character >> 4]);
          escaped.push_back(digits[character & 0x0fU]);
        } else {
          escaped.push_back(static_cast<char>(character));
        }
    }
  }
  return escaped;
}

template <typename Bytes>
std::string hex(const Bytes& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    out.push_back(digits[byte >> 4]);
    out.push_back(digits[byte & 15]);
  }
  return out;
}

std::string nonce_hex(std::uint32_t nonce) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out(8, '0');
  for (int i = 7; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = digits[nonce & 15];
    nonce >>= 4;
  }
  return out;
}

bool clean_events(const OfflineStratumController::WorkEvents& received) {
  return received.framing_rejected == 0 && received.ignored == 0;
}

std::string_view search_status_name(SearchStatus status) {
  switch (status) {
    case SearchStatus::Found: return "found";
    case SearchStatus::Exhausted: return "exhausted";
    case SearchStatus::Timeout: return "timeout";
    case SearchStatus::Cancelled: return "cancelled";
    case SearchStatus::Invalid: return "invalid";
  }
  return "invalid";
}

bool receive_event(Socket socket, OfflineStratumController& controller,
                   std::uint64_t generation,
                   OfflineStratumController::WorkEvents& received,
                   Deadline deadline = Deadline::max()) {
  auto result = pepepow::socket_io::receive_controller_line(
      socket, controller, generation, deadline, &stop_requested);
  if (!result) return false;
  received = *result;
  if (clean_events(received)) return true;
  controller.disconnected();
  return false;
}

bool wait_handshake_reply(Socket socket, OfflineStratumController& controller,
                          std::uint64_t generation, Deadline deadline,
                          std::size_t OfflineStratumController::WorkEvents::* reply) {
  OfflineStratumController::WorkEvents received;
  while (receive_event(socket, controller, generation, received, deadline)) {
    if (received.*reply == 1) return true;
  }
  return false;
}

bool handshake(Socket socket, OfflineStratumController& controller,
               std::uint64_t generation, std::string_view login,
               std::string_view transport, Clock::time_point runtime_deadline) {
  using Events = OfflineStratumController::WorkEvents;
  using namespace pepepow::socket_io;
  const auto subscribe_deadline = (std::min)(Clock::now() + request_budget, runtime_deadline);
  const auto subscribe = controller.make_subscribe_request();
  if (!subscribe || !send_before(socket, *subscribe, subscribe_deadline, stop_requested) ||
      !wait_handshake_reply(socket, controller, generation, subscribe_deadline, &Events::subscribed))
    return false;
  const auto authorize_deadline = (std::min)(Clock::now() + request_budget, runtime_deadline);
  const auto authorize = controller.make_authorize_request(login);
  if (!authorize || !send_before(socket, *authorize, authorize_deadline, stop_requested) ||
      !wait_handshake_reply(socket, controller, generation, authorize_deadline, &Events::authorized) ||
      !controller.session().jobs().is_authorized())
    return false;
  event("authorized", "\"transport\":\"" + std::string(transport) + "\"");
  return true;
}

bool set_nonblocking(Socket socket) {
#ifdef _WIN32
  u_long enabled = 1;
  return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
  const int flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

struct Totals {
  std::uint64_t accepted = 0;
  std::uint64_t rejected = 0;
  std::uint64_t pool_errors = 0;
  std::uint64_t stale = 0;
  std::uint64_t invalid = 0;
  std::uint64_t low_difficulty = 0;
  std::uint64_t duplicate = 0;
  std::uint64_t stale_replies = 0;
  std::uint64_t other_errors = 0;
  std::uint64_t hashes = 0;
  double accepted_wire_difficulty = 0;
  Clock::duration search_elapsed{};
  std::unordered_set<std::string> submitted;
  std::uint64_t responses() const { return accepted + rejected + pool_errors; }
};

pepepow::SearchResult search_live_batch(
    pepepow::CpuWorkerPool& cpu, const pepepow::Header& header,
    const pepepow::Digest& target, std::uint32_t start, std::uint32_t count,
    pepepow::SearchClock::time_point deadline, const std::atomic_bool* cancel) {
  constexpr std::uint32_t max_batch_size = 10000000;
  constexpr std::uint32_t max_search_slice = 10000;
  const auto range_end = static_cast<std::uint64_t>(start) + count;
  if (count == 0 || count > max_batch_size || range_end > (std::uint64_t{1} << 32))
    return {SearchStatus::Invalid, std::nullopt};

  std::uint32_t completed = 0;
  while (completed < count) {
    const auto slice = (std::min)(max_search_slice, count - completed);
    auto result = cpu.search(header, target, start + completed, slice, deadline, cancel);
    if (result.hashes > count - completed)
      return {SearchStatus::Invalid, std::nullopt, completed};
    completed += result.hashes;
    if (result.status != SearchStatus::Exhausted) {
      result.hashes = completed;
      return result;
    }
    if (result.hashes < slice)
      return {SearchStatus::Invalid, std::nullopt, completed};
  }
  return {SearchStatus::Exhausted, std::nullopt, completed};
}

int mine_session(Socket socket, OfflineStratumController& controller,
                 const Options& options, Totals& totals, pepepow::CpuWorkerPool& cpu,
                 std::string_view transport, Clock::time_point runtime_deadline) {
  using namespace pepepow::socket_io;
  const auto generation = controller.session().session_generation();
  if (!handshake(socket, controller, generation, options.login, transport, runtime_deadline)) {
    std::cerr << "Local Stratum subscribe/authorize handshake failed\n";
    if (options.live_pool) event("handshake_failed", "\"transport\":\"" +
                                  std::string(transport) + "\"");
    return 1;
  }
  int exit_code = 1;
  std::optional<std::uint64_t> attempted_job_generation;
  std::optional<std::uint64_t> cursor_job_generation;
  pepepow::StratumNonceCursor nonce_cursor;
  std::string extranonce2 = options.extranonce2;
  const std::string initial_extranonce2 = options.extranonce2;
  auto next_submit_at = Clock::time_point::min();
  bool failed = false;
  while (totals.responses() < options.response_goal && !failed && !stop_requested &&
         Clock::now() < runtime_deadline) {
    auto job = controller.session().jobs().active();
    if (!job || !controller.session().active_notify()) {
      OfflineStratumController::WorkEvents incoming;
      if (!receive_event(socket, controller, generation, incoming, runtime_deadline)) {
        failed = true;
        break;
      }
      if (incoming.applied != 0) event("work_received");
      continue;
    }
    if (!options.live_pool && attempted_job_generation == job->generation) {
      OfflineStratumController::WorkEvents incoming;
      if (!receive_event(socket, controller, generation, incoming, runtime_deadline)) {
        failed = true;
        break;
      }
      if (incoming.applied != 0) event("work_received");
      continue;
    }

    if (options.live_pool && cursor_job_generation != job->generation) {
      cursor_job_generation = job->generation;
      nonce_cursor.reset();
      extranonce2 = initial_extranonce2;
    }
    if (options.live_pool && nonce_cursor.exhausted()) {
      if (!pepepow::increment_stratum_extranonce2(extranonce2)) {
        event("nonce_space_exhausted", "\"job_id\":\"" + job->id + "\"");
        exit_code = 3;
        failed = true;
        break;
      }
      nonce_cursor.reset();
      event("extranonce2_advanced", "\"job_id\":\"" + job->id + "\"");
    }

    std::uint32_t batch_start = options.start;
    std::uint32_t batch_count = options.count;
    if (options.live_pool) {
      const auto batch = nonce_cursor.reserve(options.count);
      if (!batch) {
        event("nonce_cursor_error", "\"job_id\":\"" + job->id + "\"");
        exit_code = 3;
        failed = true;
        break;
      }
      batch_start = batch->start;
      batch_count = batch->count;
    }

    auto work = pepepow::OfflineStratumSearch::capture(
        controller, extranonce2, options.wire_scale);
    if (!work) {
      std::cerr << "Current job cannot produce a valid header and share target\n";
      exit_code = 2;
      failed = true;
      break;
    }

    std::atomic_bool cancel{false};
    std::atomic_bool finished{false};
    SearchStatus search_status = SearchStatus::Invalid;
    std::string cuda_diagnostic;
    const auto search_started = Clock::now();
    const auto search_deadline = options.live_pool ? runtime_deadline :
        (std::min)(search_started + std::chrono::milliseconds(options.timeout_ms), runtime_deadline);
    std::thread worker([&] {
      search_status = work->search_with([&](const auto& header, const auto& target) {
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
        if (options.live_pool && options.cuda_device) {
          pepepow::CudaSearchProcessOptions cuda_options;
          cuda_options.engine = options.cuda_engine;
          cuda_options.cpu_oracle = options.cuda_cpu_oracle;
          cuda_options.batch_size = options.cuda_batch_size;
          return pepepow::search_cuda_with_process(
              cpu, cuda_options, header, target, batch_start, batch_count,
              search_deadline, &cancel, &cuda_diagnostic);
        }
#endif
        if (options.live_pool)
          return search_live_batch(cpu, header, target, batch_start, batch_count,
                                   search_deadline, &cancel);
        return cpu.search(header, target, batch_start, batch_count,
                          search_deadline, &cancel);
      });
      finished.store(true, std::memory_order_release);
    });
    event("search_started", "\"job_id\":\"" + job->id + "\",\"generation\":" +
                                std::to_string(job->generation));

    while (!finished.load(std::memory_order_acquire) &&
           !stop_requested && Clock::now() < search_deadline) {
      const auto ready = socket_ready(socket, 1);
      if (ready == Readiness::Timeout) continue;
      if (ready == Readiness::Error) {
        controller.disconnected();
        failed = true;
        break;
      }
      OfflineStratumController::WorkEvents incoming;
      if (!receive_event(socket, controller, generation, incoming, search_deadline)) {
        cancel.store(true, std::memory_order_release);
        failed = true;
        break;
      }
      if (incoming.applied != 0) event("work_received");
      if (!controller.session().jobs().can_submit(*job)) {
        cancel.store(true, std::memory_order_release);
        event("search_cancel_requested", "\"job_id\":\"" + job->id + "\"");
        break;
      }
    }
    if (failed || stop_requested || Clock::now() >= search_deadline)
      cancel.store(true, std::memory_order_release);
    worker.join();
    const auto elapsed = Clock::now() - search_started;
    totals.hashes += work->hashes();
    totals.search_elapsed += elapsed;
    const auto seconds = std::chrono::duration<double>(elapsed).count();
    const auto total_seconds = std::chrono::duration<double>(totals.search_elapsed).count();
    event("search_metrics", "\"workers\":" + std::to_string(std::min(options.workers, options.count)) +
                            ",\"hashes\":" + std::to_string(work->hashes()) +
                            ",\"total_hashes\":" + std::to_string(totals.hashes) +
                            ",\"elapsed_ms\":" + std::to_string(seconds * 1000) +
                            ",\"local_hashrate\":" + std::to_string(seconds > 0 ? work->hashes() / seconds : 0) +
                            ",\"average_local_hashrate\":" +
                            std::to_string(total_seconds > 0 ? totals.hashes / total_seconds : 0) +
                            ",\"job_generation\":" + std::to_string(job->generation) +
                            ",\"connection_generation\":" + std::to_string(generation) +
                            ",\"reason\":\"" + (stop_requested ? "shutdown" : failed ? "transport" :
                                 !controller.session().jobs().can_submit(*job) ? "job_replaced" :
                                 std::string(search_status_name(search_status))) + "\"");
    if (stop_requested) break;
    if (Clock::now() >= search_deadline &&
        (failed || controller.session().jobs().can_submit(*job))) {
      event(options.live_pool ? "runtime_limit" : "timeout",
            "\"job_id\":\"" + job->id + "\"");
      exit_code = 4;
      failed = true;
    }
    if (failed || stop_requested) break;

    // Drain complete frames already queued before registering a submit.
    // The controller still rechecks job generation at request creation.
    while (!stop_requested && Clock::now() < search_deadline) {
      const auto ready = socket_ready(socket, 0);
      if (ready == Readiness::Timeout) break;
      if (ready == Readiness::Error) {
        controller.disconnected();
        failed = true;
        break;
      }
      OfflineStratumController::WorkEvents incoming;
      if (!receive_event(socket, controller, generation, incoming, search_deadline)) {
        failed = true;
        break;
      }
      if (incoming.applied != 0) event("work_received");
      if (!controller.session().jobs().can_submit(*job)) break;
    }
    if (stop_requested) break;
    if (Clock::now() >= search_deadline &&
        (failed || controller.session().jobs().can_submit(*job))) {
      event("timeout", "\"job_id\":\"" + job->id + "\"");
      exit_code = 4;
      failed = true;
    }
    if (failed) break;

    if (!controller.session().jobs().can_submit(*job)) {
      ++totals.stale;
      attempted_job_generation = job->generation;
      event("stale_candidate", "\"job_id\":\"" + job->id +
                                "\",\"search_status\":\"" +
                                std::string(search_status_name(search_status)) +
                                "\",\"stale_candidates\":" +
                                std::to_string(totals.stale));
      continue;
    }
    if (search_status == SearchStatus::Timeout) {
      event("timeout", "\"job_id\":\"" + job->id + "\"");
      exit_code = 4;
      failed = true;
      break;
    }
    if (search_status == SearchStatus::Cancelled) {
      std::cerr << "Search cancelled without a job replacement\n";
      failed = true;
      break;
    }
    if (search_status == SearchStatus::Exhausted) {
      if (options.live_pool) {
        if (!nonce_cursor.advance_exhausted_batch(batch_count)) {
          event("nonce_cursor_error", "\"job_id\":\"" + job->id + "\"");
          exit_code = 3;
          failed = true;
          break;
        }
        event("nonce_batch_exhausted", "\"job_id\":\"" + job->id +
                                         "\",\"next_nonce\":" +
                                         std::to_string(nonce_cursor.next_nonce()));
      } else {
        attempted_job_generation = job->generation;
        event("range_exhausted", "\"job_id\":\"" + job->id + "\"");
      }
      continue;
    }
    const auto* candidate = work->candidate();
    if (search_status != SearchStatus::Found || !candidate) {
      if (options.cuda_device && !cuda_diagnostic.empty()) {
        event("search_error", "\"device\":\"cuda\",\"code\":\"engine_failure\",\"message\":\"" +
                                  json_escape(cuda_diagnostic) + "\"");
        std::cerr << "CUDA search failed: " << cuda_diagnostic << '\n';
      } else {
        std::cerr << "Native CPU search returned an invalid state\n";
      }
      failed = true;
      break;
    }
    if (options.live_pool && !nonce_cursor.advance_after_candidate(candidate->nonce)) {
      event("nonce_cursor_error", "\"job_id\":\"" + job->id + "\"");
      exit_code = 3;
      failed = true;
      break;
    }

    event("candidate", "\"job_id\":\"" + job->id + "\",\"nonce\":\"" +
                       nonce_hex(candidate->nonce) + "\",\"header_hex\":\"" +
                       hex(candidate->header) + "\",\"digest_raw\":\"" +
                       hex(candidate->digest) + "\"");

    // A failed/ambiguous write must never be replayed after reconnect.
    const auto key = job->id + ':' + hex(candidate->header);
    if (!totals.submitted.insert(key).second) {
      if (!options.live_pool) attempted_job_generation = job->generation;
      event("duplicate_candidate");
      continue;
    }

    if (options.live_pool && Clock::now() < next_submit_at) {
      bool candidate_stale = false;
      while (!stop_requested && Clock::now() < runtime_deadline &&
             Clock::now() < next_submit_at) {
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
            next_submit_at - Clock::now());
        const auto ready = socket_ready(socket, static_cast<int>((std::min)(
            remaining, std::chrono::milliseconds(25)).count()));
        if (ready == Readiness::Timeout) continue;
        if (ready == Readiness::Error) {
          controller.disconnected();
          failed = true;
          break;
        }
        OfflineStratumController::WorkEvents incoming;
        if (!receive_event(socket, controller, generation, incoming, runtime_deadline)) {
          failed = true;
          break;
        }
        if (incoming.applied != 0) event("work_received");
        if (!controller.session().jobs().can_submit(*job)) {
          candidate_stale = true;
          break;
        }
      }
      if (failed || stop_requested) break;
      if (candidate_stale) {
        ++totals.stale;
        event("stale_candidate", "\"job_id\":\"" + job->id +
                                  "\",\"stale_candidates\":" +
                                  std::to_string(totals.stale));
        continue;
      }
      if (Clock::now() >= runtime_deadline) break;
    }

    auto submit = work->make_mock_submit(controller, options.login);
    if (!submit) {
      // A notify may invalidate a candidate in the narrow interval after
      // the socket drain and before the controller creates the request.
      if (!controller.session().jobs().can_submit(*job)) {
        ++totals.stale;
        if (!options.live_pool) attempted_job_generation = job->generation;
        event("stale_candidate", "\"job_id\":\"" + job->id +
                                  "\",\"search_status\":\"found\","
                                  "\"stale_candidates\":" +
                                  std::to_string(totals.stale));
        continue;
      }
      std::cerr << "Cannot register a submit for the current job\n";
      failed = true;
      break;
    }
    const auto submit_deadline = (std::min)(Clock::now() + request_budget, runtime_deadline);
    if (!send_before(socket, *submit, submit_deadline, stop_requested)) {
      controller.disconnected();
      std::cerr << "Local mock submit write failed\n";
      failed = true;
      break;
    }
    if (!options.live_pool) attempted_job_generation = job->generation;
    else next_submit_at = Clock::now() + std::chrono::milliseconds(options.submit_interval_ms);
    event("submit_sent", "\"job_id\":\"" + job->id + "\",\"nonce\":\"" +
                          nonce_hex(candidate->nonce) + "\"");

    const auto counters_before = controller.session().share_counters();
    while (controller.pending_status().submit_pending != 0) {
      OfflineStratumController::WorkEvents incoming;
      if (!receive_event(socket, controller, generation, incoming, submit_deadline)) {
        failed = true;
        break;
      }
      if (incoming.applied != 0) event("work_received");
      if (incoming.shares_accepted) {
        event("share_accepted", "\"count\":" + std::to_string(incoming.shares_accepted) +
                                 ",\"wire_difficulty\":" + std::to_string(job->difficulty));
        totals.accepted += incoming.shares_accepted;
        const auto weighted_difficulty =
            (job->difficulty / options.wire_scale) * 65536.0;
        if (std::isfinite(weighted_difficulty) && weighted_difficulty > 0)
          totals.accepted_wire_difficulty += weighted_difficulty * incoming.shares_accepted;
      }
      if (incoming.shares_rejected) {
        event("share_rejected");
        totals.rejected += incoming.shares_rejected;
      }
      if (incoming.shares_pool_errors) {
        event("share_pool_error");
        totals.pool_errors += incoming.shares_pool_errors;
      }
      if (incoming.shares_invalid) {
        event("share_classification", "\"class\":\"invalid\",\"count\":" +
              std::to_string(incoming.shares_invalid));
        totals.invalid += incoming.shares_invalid;
      }
      if (incoming.shares_low_difficulty) {
        event("share_classification", "\"class\":\"low_difficulty\",\"count\":" +
              std::to_string(incoming.shares_low_difficulty));
        totals.low_difficulty += incoming.shares_low_difficulty;
      }
      if (incoming.shares_duplicate) {
        event("share_classification", "\"class\":\"duplicate\",\"count\":" +
              std::to_string(incoming.shares_duplicate));
        totals.duplicate += incoming.shares_duplicate;
      }
      if (incoming.shares_stale) {
        event("share_classification", "\"class\":\"stale\",\"count\":" +
              std::to_string(incoming.shares_stale));
        totals.stale_replies += incoming.shares_stale;
      }
      if (incoming.shares_other_errors) {
        event("share_classification", "\"class\":\"other_pool_error\",\"count\":" +
              std::to_string(incoming.shares_other_errors));
        totals.other_errors += incoming.shares_other_errors;
      }
    }
    if (failed) break;
    const auto counters = controller.session().share_counters();
    if (counters.accepted == counters_before.accepted &&
        counters.rejected == counters_before.rejected &&
        counters.pool_errors == counters_before.pool_errors) {
      std::cerr << "Submit response did not update share accounting\n";
      failed = true;
      break;
    }
  }
  if (options.live_pool && !failed && !stop_requested &&
      Clock::now() >= runtime_deadline && totals.responses() < options.response_goal) {
    event("runtime_limit", "\"responses\":" + std::to_string(totals.responses()));
    exit_code = 4;
  }
  if (options.live_pool && Clock::now() >= runtime_deadline &&
      totals.responses() < options.response_goal && !stop_requested) {
    exit_code = 4;
    failed = false;
  }
  return !failed && totals.responses() >= options.response_goal ? 0 : exit_code;
}

int run(const Options& options) {
#ifdef _WIN32
  WSADATA winsock{};
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
    std::cerr << "Cannot initialize Winsock\n";
    return 5;
  }
#endif
  using namespace pepepow::socket_io;
  OfflineStratumController controller;
  Totals totals;
  pepepow::CpuWorkerPool cpu(options.workers);
  const auto run_started = Clock::now();
  const auto runtime_deadline = options.live_pool
      ? run_started + std::chrono::seconds(options.max_runtime_seconds)
      : Deadline::max();
  if (options.live_pool) {
    event("run_started", "\"endpoint\":\"" +
          format_stratum_tcp_endpoint(*options.endpoint) + "\",\"mode\":\"" +
          std::string(options.mode) + "\",\"threads\":" + std::to_string(options.workers) +
          ",\"response_goal\":" + std::to_string(options.response_goal) +
          ",\"max_runtime_seconds\":" + std::to_string(options.max_runtime_seconds) +
          ",\"submit_interval_ms\":" + std::to_string(options.submit_interval_ms) +
          ",\"device\":\"" + (options.cuda_device ? "cuda" : "cpu") + "\"");
  }
  int exit_code = 1;
  auto backoff = std::chrono::milliseconds(options.backoff_ms);
  bool opened_session = false;
  for (unsigned attempt = 0; attempt <= options.retries && !stop_requested; ++attempt) {
    if (Clock::now() >= runtime_deadline) {
      exit_code = 4;
      break;
    }
    unsigned port = options.port;
    std::string transport = "127.0.0.1";
    Socket socket = invalid_socket;
    if (options.live_pool) {
      const auto& endpoint = options.fallback_endpoint && attempt % 2
          ? *options.fallback_endpoint : *options.endpoint;
      transport = format_stratum_tcp_endpoint(endpoint);
      port = endpoint.port;
      const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
          runtime_deadline - Clock::now());
      socket = connect_stratum_tcp_endpoint(endpoint,
          (std::min)(remaining, std::chrono::milliseconds(2000)));
    } else {
      port = options.fallback_port && attempt % 2 ? options.fallback_port : options.port;
      socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    if (socket != invalid_socket) {
      bool connected = true;
      if (!options.live_pool) {
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(static_cast<std::uint16_t>(port));
        remote.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        connected = pepepow::cli::connect_local_with_timeout(socket, remote, 2000);
      }
      if (connected && set_nonblocking(socket) && !stop_requested) {
        controller.connected();
        event("connected", "\"transport\":\"" + transport + "\",\"port\":" + std::to_string(port) +
                           ",\"mode\":\"" + std::string(options.mode) +
                           "\",\"connection_generation\":" +
                           std::to_string(controller.session().session_generation()));
        opened_session = true;
        exit_code = mine_session(socket, controller, options, totals, cpu,
                                 transport, runtime_deadline);
        if (controller.pending_status().connected) controller.disconnected();
      } else {
        std::cerr << (options.live_pool ? "Could not connect to the Stratum TCP endpoint\n" :
                                          "Could not connect to the specified 127.0.0.1 port\n");
      }
      close_socket(socket);
    } else if (options.live_pool) {
      event("connect_failed", "\"endpoint\":\"" + transport +
                               "\",\"attempt\":" + std::to_string(attempt + 1));
      std::cerr << "Could not resolve or connect to the Stratum TCP endpoint\n";
    }
    if (exit_code != 1 || attempt == options.retries || stop_requested) break;
    event("retry_wait", "\"backoff_ms\":" + std::to_string(backoff.count()));
    const auto until = Clock::now() + backoff;
    while (!stop_requested && Clock::now() < until && Clock::now() < runtime_deadline)
      std::this_thread::sleep_until(std::min(until, Clock::now() + std::chrono::milliseconds(25)));
    backoff = std::min(backoff * 2, std::chrono::milliseconds(2000));
  }
  if (opened_session || options.live_pool) {
    const auto elapsed = std::chrono::duration<double>(Clock::now() - run_started).count();
    const auto search_seconds = std::chrono::duration<double>(totals.search_elapsed).count();
    if (options.live_pool) {
      event("summary", "\"accepted\":" + std::to_string(totals.accepted) +
                       ",\"rejected\":" + std::to_string(totals.rejected) +
                       ",\"pool_errors\":" + std::to_string(totals.pool_errors) +
                       ",\"stale_candidates\":" + std::to_string(totals.stale) +
                       ",\"invalid\":" + std::to_string(totals.invalid) +
                       ",\"low_difficulty\":" + std::to_string(totals.low_difficulty) +
                       ",\"duplicate\":" + std::to_string(totals.duplicate) +
                       ",\"stale_replies\":" + std::to_string(totals.stale_replies) +
                       ",\"other_errors\":" + std::to_string(totals.other_errors) +
                       ",\"hashes\":" + std::to_string(totals.hashes) +
                       ",\"local_hashrate_hps\":" +
                       std::to_string(search_seconds > 0 ? totals.hashes / search_seconds : 0) +
                       ",\"accepted_wire_difficulty_sum\":" +
                       std::to_string(totals.accepted_wire_difficulty) +
                       ",\"difficulty_weighted_accepted_hashrate_hps\":" +
                       std::to_string(elapsed > 0 ? totals.accepted_wire_difficulty / elapsed : 0) +
                       ",\"elapsed_seconds\":" + std::to_string(elapsed) +
                       ",\"stop_reason\":\"" +
                       (stop_requested ? "signal" : totals.responses() >= options.response_goal
                           ? "response_goal" : exit_code == 4 ? "runtime_limit" :
                             opened_session ? "transport_or_protocol" : "connect_failed") + "\"");
    } else {
      event("summary", "\"accepted\":" + std::to_string(totals.accepted) +
                       ",\"rejected\":" + std::to_string(totals.rejected) +
                       ",\"pool_errors\":" + std::to_string(totals.pool_errors) +
                       ",\"stale_candidates\":" + std::to_string(totals.stale));
    }
  }
#ifdef _WIN32
  WSACleanup();
#endif
  return stop_requested ? 130 : exit_code;
}
}  // namespace

int main(int argc, char** argv) {
  const auto process_started = pepepow::SearchClock::now();
  if (argc >= 2 && std::string_view(argv[1]) == "--benchmark")
    return pepepow::run_cpu_benchmark(argc - 1, argv + 1, process_started);
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "Usage: offline_stratum_miner PORT LOGIN START COUNT EXTRANONCE2 "
                 "[--responses N] [--max-ms N] [--scale N] [--retries N]\n"
                 "       [--fallback-port PORT] [--backoff-ms N] [--mode POOL|SOLO] [--threads N]\n"
                 "       offline_stratum_miner --benchmark [--threads N|all] [--seconds N]\n"
                 "--threads accepts 1..256 (default 1); all is available in benchmark mode.\n"
                 "Benchmark mode is offline and never opens a pool connection. Ctrl+C stops cooperatively.\n"
                 "Connects only to IPv4 127.0.0.1; requires a local mock server.\n"
                 "Search uses pinned Core HooHash; no public pool endpoint is accepted.\n";
#ifdef PEPEPOW_ENABLE_LIVE_POOL_MINER
#ifdef PEPEPOW_ENABLE_CUDA_PROCESS_SEARCH
    std::cout << "\nExplicit live mode (public TCP only; no TLS):\n"
                 "  pepepow-live-miner --mine --endpoint stratum+tcp://HOST:PORT "
                 "--login ADDRESS[.WORKER] --extranonce2 HEX --mode POOL|SOLO\n"
                 "      --threads N --responses N --max-runtime-seconds 10..1800\n"
                 "      [--batch-size N] [--submit-interval-ms 100..10000] [--scale N]\n"
                 "      [--retries N] [--fallback-endpoint stratum+tcp://HOST:PORT]\n"
                 "      [--device cpu|cuda --cuda-engine PATH --cpu-oracle PATH]\n"
                 "      [--cuda-batch-size 1..256]\n"
                 "CUDA candidates are checked again by the pinned-Core CPU path.\n"
                 "The public endpoint is contacted only with the explicit --mine command.\n";
#else
    std::cout << "\nExplicit live CPU mode (public TCP only; no TLS):\n"
                 "  pepepow-live-cpu-miner --mine --endpoint stratum+tcp://HOST:PORT "
                 "--login ADDRESS[.WORKER] --extranonce2 HEX --mode POOL|SOLO\n"
                 "      --threads N --responses N --max-runtime-seconds 10..1800\n"
                 "      [--batch-size N] [--submit-interval-ms 100..10000] [--scale N]\n"
                 "      [--retries N] [--fallback-endpoint stratum+tcp://HOST:PORT]\n"
                 "The public endpoint is contacted only with the explicit --mine command.\n";
#endif
#endif
    return 0;
  }
#ifdef PEPEPOW_ENABLE_LIVE_POOL_MINER
  if (argc >= 2 && std::string_view(argv[1]) == "--mine") {
    Options options;
    if (!parse_live_options(argc, argv, options)) {
      std::cerr << "Invalid live-mining arguments; run --help for usage\n";
      return 2;
    }
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
#ifdef SIGBREAK
    std::signal(SIGBREAK, request_stop);
#endif
    return run(options);
  }
#endif
  Options options;
  if (!parse_options(argc, argv, options)) {
    std::cerr << "Invalid arguments; run --help for usage\n";
    return 2;
  }
  std::signal(SIGINT, request_stop);
  std::signal(SIGTERM, request_stop);
  return run(options);
}
