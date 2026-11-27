#include "pepepow/stratum_endpoint.hpp"

#include <cstdlib>
#include <string>

#define CHECK(condition) do { if (!(condition)) return EXIT_FAILURE; } while (false)

int main() {
  using pepepow::socket_io::StratumTcpEndpoint;
  using pepepow::socket_io::format_stratum_tcp_endpoint;
  using pepepow::socket_io::parse_stratum_tcp_endpoint;

  StratumTcpEndpoint endpoint;
  CHECK(parse_stratum_tcp_endpoint("stratum+tcp://pool.pepepow.net:39333", endpoint));
  CHECK(endpoint.host == "pool.pepepow.net" && endpoint.port == 39333);
  CHECK(format_stratum_tcp_endpoint(endpoint) == "stratum+tcp://pool.pepepow.net:39333");
  CHECK(parse_stratum_tcp_endpoint("stratum+tcp://127.0.0.1:1", endpoint));
  CHECK(endpoint.host == "127.0.0.1" && endpoint.port == 1);
  CHECK(parse_stratum_tcp_endpoint("stratum+tcp://[2001:db8::1]:65535", endpoint));
  CHECK(endpoint.host == "2001:db8::1" && endpoint.port == 65535);
  CHECK(format_stratum_tcp_endpoint(endpoint) == "stratum+tcp://[2001:db8::1]:65535");

  for (const std::string invalid : {
           "pool.pepepow.net:39333",
           "stratum+ssl://pool.pepepow.net:39333",
           "stratum+tcp://pool.pepepow.net",
           "stratum+tcp://pool.pepepow.net:0",
           "stratum+tcp://pool.pepepow.net:65536",
           "stratum+tcp://pool.pepepow.net:+39333",
           "stratum+tcp://user@pool.pepepow.net:39333",
           "stratum+tcp://pool.pepepow.net:39333/path",
           "stratum+tcp://pool.pepepow.net:39333?token=x",
           "stratum+tcp://pool..pepepow.net:39333",
           "stratum+tcp://-pool.pepepow.net:39333",
           "stratum+tcp://pool-.pepepow.net:39333",
           "stratum+tcp://[2001:db8::1]:39333:1",
           "stratum+tcp://[2001:::1]:39333",
           "stratum+tcp://[fe80::1%eth0]:39333",
           "stratum+tcp://[not-ipv6]:39333",
           "stratum+tcp://127.0.0.999:39333"}) {
    CHECK(!parse_stratum_tcp_endpoint(invalid, endpoint));
  }
  return EXIT_SUCCESS;
}
