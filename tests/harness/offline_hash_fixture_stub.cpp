// SYNTHETIC hash for isolated CLI testing, NOT the pinned Core or HooHash parity.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
static constexpr const char* kDigests[8] = {
    "5a70c2cf2c149567ac339553674840814a5013f4b7c8c7d94f3358b7e6d17534",
    "680a8e85cd678d6ecc91639a922ad951d00b93d96ffde9f91338200082d37deb",
    "e7e57e35ed1e636878a66f640487905ec9aa1bfc44e1ed7ba9a4315bb40f31c8",
    "d381baa5a7fcc9b69ce43fb00c87ef83e5da4459fce51f10a90292f326690a8b",
    "697811c112ddef27f8d9d921d267873159ea8f50414d67e2be87c779dcb79f7c",
    "a1fc0d9c3b68f48b81f2603aff26c9ab2bcec9e5c00460c371436dfeb54d10cd",
    "0f2406ec4e3ad6928ba13935a255697f7e3542531ba109e804d7f61c7cf945f2",
    "65e0b9bdaca135ea1fe8a0ffd7d2a83f1862719b0b43cd5d12f797b68487581f",
};
extern "C" void hoohashv110(const void* data, std::size_t length, std::uint8_t out[32]) {
  const char* wait = std::getenv("PEPEPOW_TEST_HASH_DELAY_MS");
  if (wait && *wait) std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(wait)));
  std::memset(out, 0xff, 32);
  if (length != 80) return;
  const auto* h = static_cast<const std::uint8_t*>(data);
  constexpr const char* kHeaderPrefix = "0040002038e31388c54124146478ff691985eecd02610db91efbc9cd7aabca490000000007647f0508057dbf8c99ddaa87543c04e31dfe3f383e7386903d50c91728fabe830be16971e3021d";
  for (std::size_t i = 0; i < 76; ++i) {
    auto cv = [](char ch) { return ch <= '9' ? ch-'0' : ch-'a'+10; };
    if (h[i] != ((cv(kHeaderPrefix[i * 2]) << 4) | cv(kHeaderPrefix[i * 2 + 1]))) return;
  }
  if (h[77] != 0xff || h[78] != 0xff || h[79] != 0xff || h[76] < 0xf8) return;
  const char* hex = kDigests[h[76] - 0xf8];
  for (int i = 0; i < 32; ++i) {
    auto cv = [](char ch) { return ch <= '9' ? ch-'0' : ch-'a'+10; };
    out[i] = (cv(hex[i*2]) << 4) | cv(hex[i*2+1]);
  }
}
