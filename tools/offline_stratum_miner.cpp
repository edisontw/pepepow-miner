#include "pepepow/local_handshake.hpp"
#include "pepepow/offline_stratum_search.hpp"

#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {
using pepepow::OfflineStratumController;
using pepepow::SearchStatus;
using pepepow::socket_io::Socket;
using pepepow::socket_io::invalid_socket;

struct Options {
  unsigned port = 0;
  std::string login;
  std::string extranonce2;
  std::uint32_t start = 0;
  std::uint32_t count = 0;
  std::uint32_t timeout_ms = pepepow::default_search_timeout_ms;
  std::uint32_t response_goal = 1;
  double wire_scale = pepepow::OfflineStratumSearch::default_wire_difficulty_scale;
};

template <typename Integer>
bool decimal(std::string_view text, Integer& value) {
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size();
}

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
  return true;
}

bool wait_readable(Socket socket, int timeout_ms) {
  if (socket == invalid_socket || timeout_ms < 0) return false;
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(socket, &readable);
  timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
#ifdef _WIN32
  return select(0, &readable, nullptr, nullptr, &timeout) > 0;
#else
  return select(socket + 1, &readable, nullptr, nullptr, &timeout) > 0;
#endif
}

void event(std::string_view name, std::string_view fields = {}) {
  std::cout << "{\"event\":\"" << name << "\"";
  if (!fields.empty()) std::cout << ',' << fields;
  std::cout << "}\n" << std::flush;
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
                   OfflineStratumController::WorkEvents& received) {
  auto result = pepepow::socket_io::receive_controller_line(socket, controller, generation);
  if (!result) return false;
  received = *result;
  return clean_events(received);
}

bool handshake(Socket socket, OfflineStratumController& controller,
               std::uint64_t generation, std::string_view login) {
  if (!pepepow::socket_io::send_controller_subscribe(socket, controller, generation))
    return false;
  OfflineStratumController::WorkEvents received;
  if (!receive_event(socket, controller, generation, received) || received.subscribed != 1 ||
      !pepepow::socket_io::send_controller_authorize(socket, controller, generation, login))
    return false;
  if (!receive_event(socket, controller, generation, received) || received.authorized != 1)
    return false;
  event("authorized", "\"transport\":\"127.0.0.1\"");
  return true;
}

bool apply_socket_line(Socket socket, OfflineStratumController& controller,
                       std::uint64_t generation,
                       OfflineStratumController::WorkEvents& received) {
  return receive_event(socket, controller, generation, received);
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
  int exit_code = 1;
  Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  OfflineStratumController controller;
  bool connected = false;
  if (socket != invalid_socket) {
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(static_cast<std::uint16_t>(options.port));
    remote.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (pepepow::cli::connect_local_with_timeout(socket, remote, 2000) &&
        configure_timeouts(socket, 2000)) {
      controller.connected();
      connected = true;
      const auto generation = controller.session().session_generation();
      event("connected", "\"transport\":\"127.0.0.1\"");
      if (!handshake(socket, controller, generation, options.login)) {
        std::cerr << "Local Stratum subscribe/authorize handshake failed\n";
      } else {
        std::optional<std::uint64_t> attempted_job_generation;
        std::uint64_t stale_candidates = 0;
        std::uint64_t response_count = 0;
        bool failed = false;
        while (response_count < options.response_goal && !failed) {
          auto job = controller.session().jobs().active();
          if (!job || !controller.session().active_notify()) {
            OfflineStratumController::WorkEvents incoming;
            if (!apply_socket_line(socket, controller, generation, incoming)) {
              failed = true;
              break;
            }
            if (incoming.applied != 0) event("work_received");
            continue;
          }
          if (attempted_job_generation == job->generation) {
            OfflineStratumController::WorkEvents incoming;
            if (!apply_socket_line(socket, controller, generation, incoming)) {
              failed = true;
              break;
            }
            if (incoming.applied != 0) event("work_received");
            continue;
          }

          auto work = pepepow::OfflineStratumSearch::capture(
              controller, options.extranonce2, options.wire_scale);
          if (!work) {
            std::cerr << "Current job cannot produce a valid header and share target\n";
            exit_code = 2;
            failed = true;
            break;
          }

          std::atomic_bool cancel{false};
          std::atomic_bool finished{false};
          SearchStatus search_status = SearchStatus::Invalid;
          std::thread worker([&] {
            search_status = work->search(options.start, options.count,
                                        options.timeout_ms, &cancel);
            finished.store(true, std::memory_order_release);
          });
          event("search_started", "\"job_id\":\"" + job->id + "\",\"generation\":" +
                                      std::to_string(job->generation));

          while (!finished.load(std::memory_order_acquire)) {
            if (!wait_readable(socket, 10)) continue;
            OfflineStratumController::WorkEvents incoming;
            if (!apply_socket_line(socket, controller, generation, incoming)) {
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
          worker.join();
          if (failed) break;

          // Drain complete frames already queued before registering a submit.
          // The controller still rechecks job generation at request creation.
          while (wait_readable(socket, 0)) {
            OfflineStratumController::WorkEvents incoming;
            if (!apply_socket_line(socket, controller, generation, incoming)) {
              failed = true;
              break;
            }
            if (incoming.applied != 0) event("work_received");
            if (!controller.session().jobs().can_submit(*job)) break;
          }
          if (failed) break;

          if (!controller.session().jobs().can_submit(*job)) {
            ++stale_candidates;
            attempted_job_generation = job->generation;
            event("stale_candidate", "\"job_id\":\"" + job->id +
                                      "\",\"search_status\":\"" +
                                      std::string(search_status_name(search_status)) +
                                      "\",\"stale_candidates\":" +
                                      std::to_string(stale_candidates));
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
            attempted_job_generation = job->generation;
            event("range_exhausted", "\"job_id\":\"" + job->id + "\"");
            continue;
          }
          const auto* candidate = work->candidate();
          if (search_status != SearchStatus::Found || !candidate) {
            std::cerr << "Native CPU search returned an invalid state\n";
            failed = true;
            break;
          }
          event("candidate", "\"job_id\":\"" + job->id + "\",\"nonce\":\"" +
                             nonce_hex(candidate->nonce) + "\",\"header_hex\":\"" +
                             hex(candidate->header) + "\",\"digest_raw\":\"" +
                             hex(candidate->digest) + "\"");

          auto submit = work->make_mock_submit(controller, options.login);
          if (!submit) {
            // A notify may invalidate a candidate in the narrow interval after
            // the socket drain and before the controller creates the request.
            if (!controller.session().jobs().can_submit(*job)) {
              ++stale_candidates;
              attempted_job_generation = job->generation;
              event("stale_candidate", "\"job_id\":\"" + job->id +
                                        "\",\"search_status\":\"found\","
                                        "\"stale_candidates\":" +
                                        std::to_string(stale_candidates));
              continue;
            }
            std::cerr << "Cannot register a submit for the current job\n";
            failed = true;
            break;
          }
          if (!send_all(socket, *submit)) {
            controller.disconnected();
            std::cerr << "Local mock submit write failed\n";
            failed = true;
            break;
          }
          attempted_job_generation = job->generation;
          event("submit_sent", "\"job_id\":\"" + job->id + "\",\"nonce\":\"" +
                                nonce_hex(candidate->nonce) + "\"");

          const auto counters_before = controller.session().share_counters();
          while (controller.pending_status().submit_pending != 0) {
            OfflineStratumController::WorkEvents incoming;
            if (!apply_socket_line(socket, controller, generation, incoming)) {
              failed = true;
              break;
            }
            if (incoming.applied != 0) event("work_received");
            if (incoming.shares_accepted) {
              event("share_accepted");
              response_count += incoming.shares_accepted;
            }
            if (incoming.shares_rejected) {
              event("share_rejected");
              response_count += incoming.shares_rejected;
            }
            if (incoming.shares_pool_errors) {
              event("share_pool_error");
              response_count += incoming.shares_pool_errors;
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
        const auto counters = controller.session().share_counters();
        event("summary", "\"accepted\":" + std::to_string(counters.accepted) +
                         ",\"rejected\":" + std::to_string(counters.rejected) +
                         ",\"pool_errors\":" + std::to_string(counters.pool_errors) +
                         ",\"stale_candidates\":" + std::to_string(stale_candidates));
        if (!failed && response_count >= options.response_goal) exit_code = 0;
      }
    } else {
      std::cerr << "Could not connect to the specified 127.0.0.1 port\n";
    }
    if (connected) controller.disconnected();
    close_socket(socket);
  } else {
    std::cerr << "Could not create local TCP socket\n";
  }
#ifdef _WIN32
  WSACleanup();
#endif
  return exit_code;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "Usage: offline_stratum_miner PORT LOGIN START COUNT EXTRANONCE2 "
                 "[--responses N] [--max-ms N] [--scale N]\n"
                 "Connects only to IPv4 127.0.0.1; requires a local mock server.\n"
                 "Search uses pinned Core HooHash; no public pool endpoint is accepted.\n";
    return 0;
  }
  Options options;
  if (!parse_options(argc, argv, options)) {
    std::cerr << "Invalid arguments; run --help for usage\n";
    return 2;
  }
  return run(options);
}
