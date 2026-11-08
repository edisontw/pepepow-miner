#include "pepepow/stratum_request_encoder.hpp"
#include <cstdlib>
#include <limits>
#include <string>
#define CHECK(x) do { if (!(x)) return EXIT_FAILURE; } while (false)
int main() {
  using namespace pepepow;
  CHECK(!encode_subscribe_request(0));
  CHECK(encode_subscribe_request(1) ==
        "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[]}\n");
  CHECK(encode_subscribe_request(std::numeric_limits<std::uint64_t>::max()).has_value());
  CHECK(encode_authorize_request(2, "WALLET.worker") ==
        "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"WALLET.worker\",\"x\"]}\n");
  CHECK(encode_authorize_request(3, "WALLET.\\\"x", "sd=12") ==
        "{\"id\":3,\"method\":\"mining.authorize\",\"params\":[\"WALLET.\\\\\\\"x\",\"sd=12\"]}\n");
  CHECK(!encode_authorize_request(0, "wallet"));
  CHECK(!encode_authorize_request(4, ""));
  CHECK(!encode_authorize_request(4, "wallet\nworker"));
  CHECK(!encode_authorize_request(4, std::string(257, 'x')));
  CHECK(!encode_authorize_request(4, "wallet", std::string(257, 'x')));
  CHECK(!encode_authorize_request(4, "wallet\x80"));
  CHECK(encode_submit_request(5, "WALLET.worker", "job-1", "00a1ff00", 4,
                              "67500000", "12345678") ==
        "{\"id\":5,\"method\":\"mining.submit\",\"params\":[\"WALLET.worker\",\"job-1\",\"00a1ff00\",\"67500000\",\"12345678\"]}\n");
  CHECK(!encode_submit_request(0, "wallet", "job", "0102", 2, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 4, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0x02", 2, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 0, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 33, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 2, "6750000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 2, "6750000g", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", "job", "0102", 2, "67500000", "1234567/"));
  CHECK(!encode_submit_request(5, "wallet", "", "0102", 2, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet\r\n", "job", "0102", 2, "67500000", "12345678"));
  CHECK(!encode_submit_request(5, "wallet", std::string(257, 'j'), "0102", 2,
                               "67500000", "12345678"));
  return EXIT_SUCCESS;
}
