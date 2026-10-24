#include <iostream>
#include <string>

/* The iostreams that fit everywhere, in tiny and compact at -O0 too:
 * cout and cerr with strings, characters and integers. Prints ALL PASSED. */
int main() {
  std::string who = "iostreams";
  std::cout << "hello from " << who << ' ' << 42 << ' ' << -7L << ' ' << std::hex << 255 << std::dec << std::endl;
  std::cerr << "to stderr\n";
  std::cout << "ALL PASSED\n";
  return 0;
}
