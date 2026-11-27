#include <iostream>

int main() {
  std::cerr << "mock CUDA engine: init \"failed\" at C:\\gpu\\driver\n";
  std::cerr << "invalid=" << static_cast<char>(0xff);
  return 42;
}
