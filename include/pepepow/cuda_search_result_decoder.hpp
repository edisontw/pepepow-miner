#pragma once

#include "pepepow/bounded_cpu_search.hpp"
#include "pepepow/stratum_submit_decoder.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace pepepow {
namespace detail {

class CudaSearchJsonCursor {
 public:
  explicit CudaSearchJsonCursor(std::string_view input) : input_(input) {}

  bool take(char value) {
    whitespace();
    if (position_ == input_.size() || input_[position_] != value) return false;
    ++position_;
    return true;
  }

  bool string(std::string_view& value) {
    whitespace();
    if (position_ == input_.size() || input_[position_] != '"') return false;
    const std::size_t start = ++position_;
    while (position_ < input_.size()) {
      const char current = input_[position_++];
      if (current == '"') {
        value = input_.substr(start, position_ - start - 1);
        return true;
      }
      if (current == '\\') {
        skip_escape();
        return false;
      }
    }
    return false;
  }

  bool unsigned_integer(std::uint64_t& value) {
    whitespace();
    const std::size_t start = position_;
    while (position_ < input_.size() && input_[position_] >= '0' &&
           input_[position_] <= '9') ++position_;
    if (start == position_) return false;
    const auto parsed = std::from_chars(input_.data() + start,
                                        input_.data() + position_, value);
    return parsed.ec == std::errc{} && parsed.ptr == input_.data() + position_;
  }

  bool null_value() {
    whitespace();
    constexpr std::string_view literal = "null";
    if (input_.substr(position_, literal.size()) != literal) return false;
    position_ += literal.size();
    return true;
  }

  bool skip_value(unsigned depth = 0) {
    whitespace();
    if (position_ == input_.size() || depth > 32) return false;
    const char first = input_[position_];
    if (first == '"') return skip_string();
    if (first == '{') return skip_object(depth + 1);
    if (first == '[') return skip_array(depth + 1);
    const std::size_t start = position_;
    while (position_ < input_.size()) {
      const char current = input_[position_];
      if (current == ',' || current == '}' || current == ']' ||
          current == ' ' || current == '\t' || current == '\r' ||
          current == '\n') break;
      ++position_;
    }
    return position_ != start;
  }

  void whitespace() {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\t' ||
            input_[position_] == '\r' || input_[position_] == '\n'))
      ++position_;
  }

  bool at_end() {
    whitespace();
    return position_ == input_.size();
  }

 private:
  bool skip_string() {
    if (input_[position_++] != '"') return false;
    while (position_ < input_.size()) {
      const char current = input_[position_++];
      if (current == '"') return true;
      if (current == '\\') {
        if (position_ == input_.size()) return false;
        ++position_;
      }
    }
    return false;
  }

  void skip_escape() {
    if (position_ == input_.size()) return;
    const char escape = input_[position_++];
    if (escape == 'u' && input_.size() - position_ >= 4) position_ += 4;
  }

  bool skip_object(unsigned depth) {
    ++position_;
    whitespace();
    if (position_ < input_.size() && input_[position_] == '}') {
      ++position_;
      return true;
    }
    while (position_ < input_.size()) {
      if (!skip_string() || !take(':') || !skip_value(depth)) return false;
      whitespace();
      if (position_ < input_.size() && input_[position_] == '}') {
        ++position_;
        return true;
      }
      if (!take(',')) return false;
    }
    return false;
  }

  bool skip_array(unsigned depth) {
    ++position_;
    whitespace();
    if (position_ < input_.size() && input_[position_] == ']') {
      ++position_;
      return true;
    }
    while (position_ < input_.size()) {
      if (!skip_value(depth)) return false;
      whitespace();
      if (position_ < input_.size() && input_[position_] == ']') {
        ++position_;
        return true;
      }
      if (!take(',')) return false;
    }
    return false;
  }

  std::string_view input_;
  std::size_t position_ = 0;
};

struct CudaSearchDecodedCandidate {
  std::uint32_t nonce = 0;
  Digest digest{};
};

inline bool decode_digest_hex(std::string_view text, Digest& digest) {
  if (text.size() != digest.size() * 2) return false;
  auto nibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0; index < digest.size(); ++index) {
    const int high = nibble(text[index * 2]);
    const int low = nibble(text[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    digest[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

inline bool decode_cuda_candidate(CudaSearchJsonCursor& cursor,
                                  CudaSearchDecodedCandidate& candidate) {
  if (!cursor.take('{')) return false;
  bool have_nonce = false;
  bool have_digest = false;
  std::uint64_t nonce = 0;
  std::string_view digest_hex;
  if (cursor.take('}')) return false;
  while (true) {
    std::string_view key;
    if (!cursor.string(key) || !cursor.take(':')) return false;
    if (key == "nonce") {
      if (have_nonce || !cursor.unsigned_integer(nonce) ||
          nonce > std::numeric_limits<std::uint32_t>::max()) return false;
      have_nonce = true;
    } else if (key == "digest_hex") {
      if (have_digest || !cursor.string(digest_hex)) return false;
      have_digest = true;
    } else if (!cursor.skip_value(1)) {
      return false;
    }
    if (cursor.take('}')) break;
    if (!cursor.take(',')) return false;
  }
  candidate.nonce = static_cast<std::uint32_t>(nonce);
  return have_nonce && have_digest && decode_digest_hex(digest_hex, candidate.digest);
}

}  // namespace detail

// Decode one stable bounded-search JSON result and bind it to the exact request
// range. The live session uses slices smaller than UINT32_MAX so SearchResult's
// hash counter remains representable.
inline std::optional<SearchResult> decode_cuda_search_result(
    std::string_view json, const Header& header, std::uint32_t expected_start,
    std::uint64_t expected_count) {
  constexpr std::size_t max_json_bytes = 1U << 20;
  constexpr std::size_t max_candidates = 256U;
  constexpr std::uint64_t nonce_space = std::uint64_t{1} << 32;
  if (json.empty() || json.size() > max_json_bytes || expected_count == 0 ||
      expected_count > std::numeric_limits<std::uint32_t>::max() ||
      expected_count > nonce_space - expected_start) return std::nullopt;

  std::size_t valid_end = 0;
  JsonValueValidator validator(json);
  if (!validator.parse(valid_end)) return std::nullopt;
  while (valid_end < json.size() &&
         (json[valid_end] == ' ' || json[valid_end] == '\t' ||
          json[valid_end] == '\r' || json[valid_end] == '\n')) ++valid_end;
  if (valid_end != json.size()) return std::nullopt;

  detail::CudaSearchJsonCursor cursor(json);
  if (!cursor.take('{')) return std::nullopt;

  bool have_schema = false, have_status = false, have_search_status = false;
  bool have_stop_reason = false, have_nonce_start = false, have_nonce_count = false;
  bool have_processed = false, have_next = false, have_batch_size = false;
  bool have_batch_count = false, have_engine = false, have_target_encoding = false;
  bool have_cpu_recheck = false, have_candidates = false, have_error = false;
  std::uint64_t schema = 0, nonce_start = 0, nonce_count = 0, processed = 0;
  std::uint64_t next_nonce = 0, batch_size = 0, batch_count = 0;
  std::string_view status, search_status, engine, target_encoding, cpu_recheck;
  std::string_view stop_reason;
  bool stop_reason_is_null = false;
  std::vector<detail::CudaSearchDecodedCandidate> candidates;

  if (cursor.take('}')) return std::nullopt;
  while (true) {
    std::string_view key;
    if (!cursor.string(key) || !cursor.take(':')) return std::nullopt;
    if (key == "schema_version") {
      if (have_schema || !cursor.unsigned_integer(schema)) return std::nullopt;
      have_schema = true;
    } else if (key == "status") {
      if (have_status || !cursor.string(status)) return std::nullopt;
      have_status = true;
    } else if (key == "search_status") {
      if (have_search_status || !cursor.string(search_status)) return std::nullopt;
      have_search_status = true;
    } else if (key == "stop_reason") {
      if (have_stop_reason) return std::nullopt;
      if (cursor.null_value()) stop_reason_is_null = true;
      else if (!cursor.string(stop_reason)) return std::nullopt;
      have_stop_reason = true;
    } else if (key == "nonce_start") {
      if (have_nonce_start || !cursor.unsigned_integer(nonce_start)) return std::nullopt;
      have_nonce_start = true;
    } else if (key == "nonce_count") {
      if (have_nonce_count || !cursor.unsigned_integer(nonce_count)) return std::nullopt;
      have_nonce_count = true;
    } else if (key == "processed_count") {
      if (have_processed || !cursor.unsigned_integer(processed)) return std::nullopt;
      have_processed = true;
    } else if (key == "next_nonce") {
      if (have_next || !cursor.unsigned_integer(next_nonce)) return std::nullopt;
      have_next = true;
    } else if (key == "batch_size") {
      if (have_batch_size || !cursor.unsigned_integer(batch_size)) return std::nullopt;
      have_batch_size = true;
    } else if (key == "batch_count") {
      if (have_batch_count || !cursor.unsigned_integer(batch_count)) return std::nullopt;
      have_batch_count = true;
    } else if (key == "engine") {
      if (have_engine || !cursor.string(engine)) return std::nullopt;
      have_engine = true;
    } else if (key == "target_encoding") {
      if (have_target_encoding || !cursor.string(target_encoding)) return std::nullopt;
      have_target_encoding = true;
    } else if (key == "cpu_recheck") {
      if (have_cpu_recheck || !cursor.string(cpu_recheck)) return std::nullopt;
      have_cpu_recheck = true;
    } else if (key == "candidates") {
      if (have_candidates || !cursor.take('[')) return std::nullopt;
      have_candidates = true;
      if (!cursor.take(']')) {
        while (true) {
          if (candidates.size() == max_candidates) return std::nullopt;
          detail::CudaSearchDecodedCandidate candidate;
          if (!detail::decode_cuda_candidate(cursor, candidate)) return std::nullopt;
          candidates.push_back(candidate);
          if (cursor.take(']')) break;
          if (!cursor.take(',')) return std::nullopt;
        }
      }
    } else if (key == "error") {
      if (have_error || !cursor.null_value()) return std::nullopt;
      have_error = true;
    } else if (!cursor.skip_value(0)) {
      return std::nullopt;
    }
    if (cursor.take('}')) break;
    if (!cursor.take(',')) return std::nullopt;
  }
  if (!cursor.at_end() || schema != 1 || !have_schema || !have_status ||
      !have_search_status || !have_stop_reason || !have_nonce_start ||
      !have_nonce_count || !have_processed || !have_next || !have_batch_size ||
      !have_batch_count || !have_engine || !have_target_encoding ||
      !have_cpu_recheck || !have_candidates || !have_error ||
      engine != "cuda_hoohash_v110_bounded_search" ||
      target_encoding != "little_endian_uint256" ||
      cpu_recheck != "pinned_core_full_digest_and_target" ||
      nonce_start != expected_start || nonce_count != expected_count ||
      processed > expected_count ||
      next_nonce != static_cast<std::uint64_t>(expected_start) + processed ||
      batch_size == 0 || batch_size > 256 || batch_count > processed)
    return std::nullopt;
  const auto expected_batches =
      processed == 0 ? 0 : (processed + batch_size - 1) / batch_size;
  if (batch_count != expected_batches || candidates.size() > batch_size)
    return std::nullopt;

  const auto range_end = static_cast<std::uint64_t>(expected_start) + processed;
  std::uint32_t previous_nonce = 0;
  bool first_candidate = true;
  for (const auto& candidate : candidates) {
    if (candidate.nonce < expected_start ||
        static_cast<std::uint64_t>(candidate.nonce) >= range_end ||
        (!first_candidate && candidate.nonce <= previous_nonce)) return std::nullopt;
    first_candidate = false;
    previous_nonce = candidate.nonce;
  }

  if (status == "ok" && search_status == "found" && stop_reason_is_null &&
      !candidates.empty() && processed != 0) {
    Header candidate_header = header;
    auto& candidate = candidates.front();
    pepepow_header_set_nonce(candidate_header.data(), candidate_header.size(),
                             candidate.nonce);
    return SearchResult{SearchStatus::Found,
                        CpuCandidate{candidate_header, candidate.digest, candidate.nonce},
                        static_cast<std::uint32_t>(processed)};
  }
  if (status == "ok" && search_status == "exhausted" && stop_reason_is_null &&
      candidates.empty() && processed == expected_count)
    return SearchResult{SearchStatus::Exhausted, std::nullopt,
                        static_cast<std::uint32_t>(processed)};
  if (status == "cancelled" && search_status == "cancelled" &&
      !stop_reason_is_null && !stop_reason.empty() && candidates.empty()) {
    const auto result_status = stop_reason == "timeout" ? SearchStatus::Timeout
                                                         : SearchStatus::Cancelled;
    return SearchResult{result_status, std::nullopt,
                        static_cast<std::uint32_t>(processed)};
  }
  return std::nullopt;
}

}  // namespace pepepow