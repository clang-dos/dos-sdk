#include <iostream>
#include <sstream>
#include <string>

/* The iostreams that fit in the models with 64 KiB of code: cout, cin and
 * cerr, string streams, integers and strings. Reads two lines from
 * standard input. Prints ALL PASSED. */
static int fails;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL " << __LINE__ << ": " #c "\n"; fails++; } } while (0)

int main() {
  std::cout << "hello from iostreams " << 42 << std::endl;

  std::ostringstream os;
  os << 42 << ' ' << -7L << ' ' << std::hex << 255 << std::dec << ' ' << true;
  CHECK(os.str() == "42 -7 ff 1");

  std::istringstream is("123 -45 word");
  int a = 0, b = 0;
  std::string w;
  is >> a >> b >> w;
  CHECK(a == 123 && b == -45 && w == "word");

  std::string in1, in2;
  std::getline(std::cin, in1);
  std::getline(std::cin, in2);
  CHECK(in1 == "first" && in2 == "second line");
  std::cerr << "to stderr\n";

  std::cout << (fails ? "FAILED" : "ALL PASSED") << '\n';
  return fails;
}
