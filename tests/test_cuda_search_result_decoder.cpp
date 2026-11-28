#include "pepepow/cuda_search_result_decoder.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

std::string result(std::string_view status, std::string_view search_status,
                   std::string_view stop_reason, std::uint32_t start,
                   std::uint64_t count, std::uint64_t processed,
                   std::uint64_t next, std::uint64_t batch_size,
                   std::uint64_t batch_count, std::string_view candidates) {
  return "{\"schema_version\":1,\"status\":\"" + std::string(status) +
      "\",\"engine\":\"cuda_hoohash_v110_bounded_search\","
      "\"search_status\":\"" + std::string(search_status) +
      "\",\"stop_reason\":" + std::string(stop_reason) +
      ",\"nonce_start\":" + std::to_string(start) +
      ",\"nonce_count\":" + std::to_string(count) +
      ",\"processed_count\":" + std::to_string(processed) +
      ",\"next_nonce\":" + std::to_string(next) +
      ",\"batch_size\":" + std::to_string(batch_size) +
      ",\"batch_count\":" + std::to_string(batch_count) +
      ",\"matrix_cache\":{\"builds\":1,\"reuses\":0},"
      "\"target_encoding\":\"little_endian_uint256\","
      "\"cpu_recheck\":\"pinned_core_full_digest_and_target\","
      "\"candidates\":[" + std::string(candidates) +
      "],\"device\":{\"name\":\"NVIDIA GeForce RTX 3060\"},"
      "\"cuda_versions\":{\"runtime_api\":13030},\"error\":null}";
}

std::string candidate(std::uint32_t nonce, std::string_view digest) {
  return "{\"nonce\":" + std::to_string(nonce) +
      ",\"digest_hex\":\"" + std::string(digest) + "\"}";
}
}  // namespace

int main() {
  using namespace pepepow;
  Header header{};
  header[0] = 0x42;
  const std::string digest =
      "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";

  const auto first = std::numeric_limits<std::uint32_t>::max() - 2U;
  auto found_json = result("ok", "found", "null", first, 3, 3,
                           std::uint64_t{1} << 32, 2, 2,
                           candidate(first, digest) + "," +
                               candidate(std::numeric_limits<std::uint32_t>::max(), digest));
  const auto found = decode_cuda_search_result(found_json, header, first, 3);
  require(found && found->status == SearchStatus::Found && found->candidate,
          "valid full-digest candidate was rejected");
  require(found->candidate->nonce == first && found->hashes == 3,
          "candidate nonce or processed count was not preserved");
  require(found->candidate->digest[0] == 0x00 &&
              found->candidate->digest[31] == 0xff,
          "the full 32-byte digest was not decoded");
  require(found->candidate->header[0] == 0x42 &&
              found->candidate->header[76] == static_cast<std::uint8_t>(first) &&
              found->candidate->header[77] == static_cast<std::uint8_t>(first >> 8) &&
              found->candidate->header[78] == static_cast<std::uint8_t>(first >> 16) &&
              found->candidate->header[79] == static_cast<std::uint8_t>(first >> 24),
          "candidate header did not carry the nonce in Core byte order");

  const auto max_nonce = std::numeric_limits<std::uint32_t>::max();
  const auto max_json = result("ok", "found", "null", max_nonce, 1, 1,
                               std::uint64_t{1} << 32, 1, 1,
                               candidate(max_nonce, digest));
  const auto max_found = decode_cuda_search_result(max_json, header, max_nonce, 1);
  require(max_found && max_found->candidate &&
              max_found->candidate->nonce == max_nonce,
          "UINT32_MAX candidate was rejected");

  const auto exhausted_json = result("ok", "exhausted", "null", 6, 2, 2, 8,
                                     2, 1, "");
  const auto exhausted = decode_cuda_search_result(exhausted_json, header, 6, 2);
  require(exhausted && exhausted->status == SearchStatus::Exhausted &&
              exhausted->hashes == 2 && !exhausted->candidate,
          "complete no-hit range was not reported as exhausted");

  const auto timeout_json = result("cancelled", "cancelled", "\"timeout\"",
                                   100, 10, 1, 101, 1, 1, "");
  const auto timeout = decode_cuda_search_result(timeout_json, header, 100, 10);
  require(timeout && timeout->status == SearchStatus::Timeout &&
              timeout->hashes == 1,
          "deadline cancellation was not mapped to timeout");

  const auto cancel_json = result("cancelled", "cancelled", "\"cancel_file\"",
                                  100, 10, 1, 101, 1, 1, "");
  const auto cancelled = decode_cuda_search_result(cancel_json, header, 100, 10);
  require(cancelled && cancelled->status == SearchStatus::Cancelled,
          "stale-job cancellation was not preserved");

  const auto whitespace_json = std::string(" \n") +
      "{ \"schema_version\" : 1, \"status\" : \"ok\", "
      "\"engine\" : \"cuda_hoohash_v110_bounded_search\", "
      "\"search_status\" : \"found\", \"stop_reason\" : null, "
      "\"nonce_start\" : 7, \"nonce_count\" : 1, "
      "\"processed_count\" : 1, \"next_nonce\" : 8, "
      "\"batch_size\" : 1, \"batch_count\" : 1, "
      "\"target_encoding\" : \"little_endian_uint256\", "
      "\"cpu_recheck\" : \"pinned_core_full_digest_and_target\", "
      "\"candidates\" : [" + candidate(7, digest) +
      "], \"device\" : {\"name\" : \"GPU\"}, \"error\" : null } \n";
  require(decode_cuda_search_result(whitespace_json, header, 7, 1).has_value(),
          "valid JSON whitespace was rejected");

  require(!decode_cuda_search_result(found_json, header, first, 2),
          "request nonce-count mismatch was accepted");
  auto wrong_next = found_json;
  const auto next_field = wrong_next.find("\"next_nonce\":4294967296");
  require(next_field != std::string::npos, "test payload next_nonce missing");
  wrong_next.replace(next_field, std::string("\"next_nonce\":4294967296").size(),
                     "\"next_nonce\":4294967295");
  require(!decode_cuda_search_result(wrong_next, header, first, 3),
          "inconsistent next nonce was accepted");
  const auto short_digest = result("ok", "found", "null", 4, 1, 1, 5, 1, 1,
                                   candidate(4, digest.substr(2)));
  require(!decode_cuda_search_result(short_digest, header, 4, 1),
          "truncated digest was accepted");
  const auto duplicate = result("ok", "found", "null", 10, 2, 2, 12, 2, 1,
                                candidate(10, digest) + "," + candidate(10, digest));
  require(!decode_cuda_search_result(duplicate, header, 10, 2),
          "duplicate candidate nonce was accepted");
  const auto out_of_range = result("ok", "found", "null", 10, 2, 1, 11, 1, 1,
                                   candidate(11, digest));
  require(!decode_cuda_search_result(out_of_range, header, 10, 2),
          "candidate outside processed range was accepted");
  auto nonce_overflow = result("ok", "found", "null", 10, 1, 1, 11, 1, 1,
                               candidate(10, digest));
  const auto nonce_field = nonce_overflow.find("\"nonce\":10");
  require(nonce_field != std::string::npos, "candidate nonce field missing");
  nonce_overflow.replace(nonce_field, std::string("\"nonce\":10").size(),
                         "\"nonce\":4294967296");
  require(!decode_cuda_search_result(nonce_overflow, header, 10, 1),
          "candidate nonce above uint32 was accepted");
  const auto cancelled_with_candidate =
      result("cancelled", "cancelled", "\"timeout\"", 10, 2, 1, 11, 1, 1,
             candidate(10, digest));
  require(!decode_cuda_search_result(cancelled_with_candidate, header, 10, 2),
          "cancelled result published a candidate");
  const auto incomplete_exhaustion =
      result("ok", "exhausted", "null", 10, 2, 1, 11, 1, 1, "");
  require(!decode_cuda_search_result(incomplete_exhaustion, header, 10, 2),
          "partial range was accepted as exhausted");
  const auto wrong_batches =
      result("ok", "exhausted", "null", 10, 2, 2, 12, 2, 2, "");
  require(!decode_cuda_search_result(wrong_batches, header, 10, 2),
          "inconsistent batch coverage was accepted");

  std::cout << "CUDA bounded-search result contract: PASS\n";
  return 0;
}
