#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  const char *oracle = std::getenv("PEPEPOW_CUDA_TEST_ORACLE");
  const char *marker = std::getenv("PEPEPOW_CUDA_TEST_ORACLE_MARKER");
  if (oracle == nullptr || *oracle == '\0' || marker == nullptr || *marker == '\0')
    return 125;

  std::ofstream marker_file(marker, std::ios::binary | std::ios::trunc);
  if (!marker_file) return 126;
  marker_file << "recheck-started\n";
  marker_file.close();
  if (!marker_file) return 126;

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

#ifdef _WIN32
  std::vector<const char *> arguments;
  arguments.reserve(static_cast<std::size_t>(argc) + 1U);
  arguments.push_back(oracle);
  for (int index = 1; index < argc; ++index) arguments.push_back(argv[index]);
  arguments.push_back(nullptr);
  const intptr_t result = _spawnv(_P_WAIT, oracle, arguments.data());
  return result < 0 ? 127 : static_cast<int>(result);
#else
  std::vector<char *> arguments;
  arguments.reserve(static_cast<std::size_t>(argc) + 1U);
  arguments.push_back(const_cast<char *>(oracle));
  for (int index = 1; index < argc; ++index) arguments.push_back(argv[index]);
  arguments.push_back(nullptr);
  execv(oracle, arguments.data());
  return 127;
#endif
}
