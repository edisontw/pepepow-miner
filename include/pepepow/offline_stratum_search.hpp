#pragma once
#include "pepepow/bounded_cpu_search.hpp"
#include "pepepow/offline_stratum_controller.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pepepow {
// This adapter produces request text for an offline mock. The controller and
// its session are driven on one thread.
class OfflineStratumSearch {
 public:
  OfflineStratumSearch(const OfflineStratumSearch&) = delete;
  OfflineStratumSearch(OfflineStratumSearch&&) = default;

  // The pinned pool config defaults to a 2^16 multiplier on advertised difficulty.
  static constexpr double default_wire_difficulty_scale = 65536.0;

  static std::optional<OfflineStratumSearch> capture(
      const OfflineStratumController& controller, std::string_view extranonce2,
      double wire_difficulty_scale = default_wire_difficulty_scale) {
    const auto& session = controller.session();
    const auto job = session.jobs().active();
    const auto* notify = session.active_notify();
    const auto& subscription = session.subscription();
    if (!job || !notify || !subscription ||
        !stratum_hex(extranonce2, subscription->extranonce2_size)) return std::nullopt;

    if (!std::isfinite(wire_difficulty_scale) || wire_difficulty_scale < 1.0)
      return std::nullopt;
    const auto target = share_target(job->difficulty / wire_difficulty_scale);
    if (!target) return std::nullopt;
    const auto coinbase = decode_hex(notify->coinb1 + subscription->extranonce1 +
                                     std::string(extranonce2) + notify->coinb2);
    auto merkle = sha256d(coinbase.data(), coinbase.size());
    for (const auto& branch : notify->merkle_branch) {
      if (!merkle) return std::nullopt;
      const auto sibling = decode_hex(branch);
      std::array<std::uint8_t, 64> pair{};
      std::copy(merkle->begin(), merkle->end(), pair.begin());
      std::copy(sibling.begin(), sibling.end(), pair.begin() + merkle->size());
      merkle = sha256d(pair.data(), pair.size());
    }

    if (!merkle) return std::nullopt;

    Header header{};
    copy_reversed(notify->version, header.data());
    // The pool's notify prevhash reverses bytes within each 32-bit word.
    for (std::size_t offset = 0; offset < 32; offset += 4)
      copy_reversed(std::string_view(notify->prevhash).substr(offset * 2, 8),
                    header.data() + 4 + offset);
    std::copy(merkle->begin(), merkle->end(), header.begin() + 36);
    copy_reversed(notify->ntime, header.data() + 68);
    copy_reversed(notify->bits, header.data() + 72);
    return OfflineStratumSearch{controller, session.session_generation(), *job, header, *target,
                                std::string(extranonce2), notify->ntime,
                                subscription->extranonce2_size};
  }

  SearchStatus search(std::uint32_t start, std::uint32_t count,
                      std::uint32_t timeout_ms = default_search_timeout_ms) {
    if (result_) return SearchStatus::Invalid;
    result_ = search_cpu(header_, target_, start, count, timeout_ms);
    return result_->status;
  }

  const CpuCandidate* candidate() const {
    return result_ && result_->candidate ? &*result_->candidate : nullptr;
  }

  std::optional<std::string> make_mock_submit(OfflineStratumController& controller,
                                              std::string_view login) {
    const auto* found = candidate();
    if (&controller != owner_ || submitted_ || !found ||
        generation_ != controller.session().session_generation()) return std::nullopt;
    char nonce[9];
    std::snprintf(nonce, sizeof(nonce), "%08llx", static_cast<unsigned long long>(found->nonce));
    auto request = controller.make_submit_request(job_, login, extranonce2_, extranonce2_size_,
                                                  ntime_, nonce);
    submitted_ = request.has_value();
    return request;
  }

 private:
  OfflineStratumSearch(const OfflineStratumController& owner, std::uint64_t generation,
                       JobSnapshot job, Header header, Digest target, std::string extranonce2,
                       std::string ntime, unsigned extranonce2_size)
      : owner_(&owner), generation_(generation), job_(std::move(job)), header_(header),
        target_(target), extranonce2_(std::move(extranonce2)), ntime_(std::move(ntime)),
        extranonce2_size_(extranonce2_size) {}

  static std::vector<std::uint8_t> decode_hex(std::string_view hex) {
    std::vector<std::uint8_t> bytes(hex.size() / 2);
    // All inputs here have already passed the Stratum hex decoders.
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      const auto* first = hex.data() + i * 2;
      std::from_chars(first, first + 2, bytes[i], 16);
    }
    return bytes;
  }

  static void copy_reversed(std::string_view hex, std::uint8_t* output) {
    const auto bytes = decode_hex(hex);
    std::reverse_copy(bytes.begin(), bytes.end(), output);
  }

  static std::optional<Digest> sha256d(const std::uint8_t* bytes, std::size_t size) {
    Digest first{}, second{};
    if (!SHA256(bytes, size, first.data()) ||
        !SHA256(first.data(), first.size(), second.data())) return std::nullopt;
    return second;
  }

  static std::optional<Digest> share_target(double difficulty) {
    // Match the pinned pool's binary64 D1 / difficulty, then truncate and clamp.
    // The pool cannot convert an infinite quotient to an integer.
    constexpr double diff1 = 0xffffp224;
    auto value = diff1 / difficulty;
    if (!std::isfinite(value)) return std::nullopt;
    Digest target{};
    if (value >= 0x1p256) {
      target.fill(0xff);
    } else {
      value = std::floor(std::max(1.0, value));
      for (auto& byte : target) {
        byte = static_cast<std::uint8_t>(std::fmod(value, 256.0));
        value = std::floor(value / 256.0);
      }
    }
    return target;
  }

  const OfflineStratumController* owner_;
  std::uint64_t generation_;
  JobSnapshot job_;
  Header header_;
  Digest target_;
  std::string extranonce2_, ntime_;
  unsigned extranonce2_size_;
  std::optional<SearchResult> result_;
  bool submitted_ = false;
};
} // namespace pepepow
