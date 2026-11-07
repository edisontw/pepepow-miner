#include "pepepow/stratum_line_framer.hpp"
#include <cassert>
#include <string>
int main() {
  pepepow::StratumLineFramer f;
  auto a = f.feed("{\"method\":\"mining.no");
  assert(a.lines.empty() && a.rejected == 0);
  auto b = f.feed("tify\"}\r\n{}\n");
  assert(b.lines.size() == 2 && b.lines[0] == "{\"method\":\"mining.notify\"}" && b.lines[1] == "{}");
  auto c = f.feed(std::string(65537, 'x'));
  assert(c.lines.empty());
  auto d = f.feed("\n{}\n");
  assert(d.rejected == 1 && d.lines.size() == 1 && d.lines[0] == "{}");
  auto e = f.feed(std::string("a\0b\n", 4));
  assert(e.rejected == 1 && e.lines.empty());
  // A raw carriage return is only valid immediately before LF.
  assert(f.feed("bad\rdata\n").rejected == 1);
  assert(f.feed("bad\r\r\n").rejected == 1);
  assert(f.feed("{}\r").lines.empty());
  assert(f.feed("\n").lines.size() == 1);

  f.feed("partial"); f.reset();
  assert(f.feed("{}\n").lines[0] == "{}");
  assert(f.feed(std::string(65536, 'x') + "\n").lines.size() == 1);
  assert(f.feed(std::string(65536, 'x') + "\r\n").lines.size() == 1);
}
