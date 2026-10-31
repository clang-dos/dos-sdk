// C++ hello world: iostream, strings and containers.
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

int main(int argc, char **argv) {
  std::vector<std::string> v{"world", "Hello,"};
  std::sort(v.begin(), v.end());
  std::cout << v[0] << ' ' << v[1] << " from " << (argc > 1 ? argv[1] : "?")
            << "! " << 42 << std::endl;
  return 0;
}
