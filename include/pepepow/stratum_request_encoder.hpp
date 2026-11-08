#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pepepow {
// Offline Stratum V1 JSON-RPC request serialization only: no I/O or hashing.
// An ID must come from the per-connection request controller.
inline bool append_json_ascii(std::string& out, std::string_view value,
                              std::size_t max_length, bool allow_empty = false) {
  if ((!allow_empty && value.empty()) || value.size() > max_length) return false;
  out.push_back('"');
  for (unsigned char c : value) {
    // Login, job ID, and protocol hex fields are deliberately ASCII-only.
    if (c < 0x20 || c > 0x7e) return false;
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(static_cast<char>(c));
  }
  out.push_back('"');
  return true;
}

inline bool stratum_hex(std::string_view hex, std::size_t expected_bytes) {
  if (hex.size() != expected_bytes * 2) return false;
  for (char c : hex) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

inline std::optional<std::string> encode_subscribe_request(std::uint64_t id) {
  if (!id) return std::nullopt;
  return "{\"id\":" + std::to_string(id) +
         ",\"method\":\"mining.subscribe\",\"params\":[]}\n";
}

inline std::optional<std::string> encode_authorize_request(
    std::uint64_t id, std::string_view login, std::string_view password = "x") {
  if (!id) return std::nullopt;
  std::string out = "{\"id\":" + std::to_string(id) +
                    ",\"method\":\"mining.authorize\",\"params\":[";
  if (!append_json_ascii(out, login, 256)) return std::nullopt;
  out.push_back(',');
  if (!append_json_ascii(out, password, 256, true)) return std::nullopt;
  out += "]}\n";
  return out;
}

// ntime and nonce are Stratum hex-number strings (8 hex digits each), NOT
// raw little-endian header bytes. extranonce2_size comes from subscribe reply.
inline std::optional<std::string> encode_submit_request(
    std::uint64_t id, std::string_view login, std::string_view job_id,
    std::string_view extranonce2, unsigned extranonce2_size,
    std::string_view ntime, std::string_view nonce) {
  if (!id || extranonce2_size == 0 || extranonce2_size > 32 ||
      !stratum_hex(extranonce2, extranonce2_size) ||
      !stratum_hex(ntime, 4) || !stratum_hex(nonce, 4)) return std::nullopt;
  std::string out = "{\"id\":" + std::to_string(id) +
                    ",\"method\":\"mining.submit\",\"params\":[";
  if (!append_json_ascii(out, login, 256)) return std::nullopt;
  out.push_back(',');
  if (!append_json_ascii(out, job_id, 256)) return std::nullopt;
  out.push_back(',');
  if (!append_json_ascii(out, extranonce2, 64)) return std::nullopt;
  out.push_back(',');
  if (!append_json_ascii(out, ntime, 8)) return std::nullopt;
  out.push_back(',');
  if (!append_json_ascii(out, nonce, 8)) return std::nullopt;
  out += "]}\n";
  return out;
}
} // namespace pepepow
