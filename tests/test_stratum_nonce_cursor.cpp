#include "pepepow/stratum_nonce_cursor.hpp"

#include <cstdlib>
#include <cstdint>
#include <string>

#define CHECK(condition) do { if (!(condition)) return EXIT_FAILURE; } while (false)

int main() {
  using pepepow::StratumNonceCursor;
  using pepepow::stratum_nonce_space_size;

  StratumNonceCursor cursor;
  auto batch = cursor.reserve(2);
  CHECK(batch && batch->start == 0 && batch->count == 2);
  CHECK(cursor.advance_exhausted_batch(batch->count));
  CHECK(cursor.next_nonce() == 2);
  CHECK(!cursor.advance_exhausted_batch(0));

  CHECK(cursor.advance_exhausted_batch(UINT32_MAX - 2));
  CHECK(cursor.next_nonce() == UINT32_MAX);
  CHECK(!cursor.advance_exhausted_batch(2));
  CHECK(cursor.next_nonce() == UINT32_MAX);
  batch = cursor.reserve(2);
  CHECK(batch && batch->start == UINT32_MAX && batch->count == 1);
  CHECK(cursor.advance_exhausted_batch(batch->count));
  CHECK(cursor.next_nonce() == stratum_nonce_space_size);
  CHECK(cursor.exhausted());
  CHECK(!cursor.reserve(1));
  CHECK(!cursor.advance_exhausted_batch(1));

  cursor.reset();
  CHECK(cursor.next_nonce() == 0 && !cursor.exhausted());
  CHECK(cursor.advance_exhausted_batch(UINT32_MAX));
  CHECK(cursor.next_nonce() == UINT32_MAX);
  CHECK(cursor.advance_after_candidate(UINT32_MAX));
  CHECK(cursor.next_nonce() == stratum_nonce_space_size && cursor.exhausted());
  CHECK(!cursor.advance_after_candidate(0));

  std::string extranonce = "fffffffe";
  CHECK(pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce == "ffffffff");
  CHECK(!pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce == "ffffffff");

  extranonce = "00ff";
  CHECK(pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce == "0100");
  extranonce = "00fF";
  CHECK(pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce == "0100");
  extranonce = "01g0";
  CHECK(!pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce == "01g0");
  extranonce.clear();
  CHECK(!pepepow::increment_stratum_extranonce2(extranonce));
  CHECK(extranonce.empty());
  return EXIT_SUCCESS;
}
