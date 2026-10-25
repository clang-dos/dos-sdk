#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/* iostreams on DOS: cout and cin, string streams, file streams, number
 * formatting and parsing, manipulators. Reads two lines from standard
 * input. Prints ALL PASSED. */
static int fails;
#define CHECK(c) do { if (!(c)) { std::cout << "FAIL " << __LINE__ << ": " #c "\n"; fails++; } } while (0)

int main() {
  std::cout << "hello from iostreams" << std::endl;

  std::ostringstream os;
  os << 42 << ' ' << -7L << ' ' << 3.5 << ' ' << std::hex << 255 << ' '
     << std::dec << std::setw(5) << std::setfill('0') << 12 << ' ' << true
     << ' ' << std::boolalpha << false;
  CHECK(os.str() == "42 -7 3.5 ff 00012 1 false");

  std::ostringstream fx;
  fx << std::fixed << std::setprecision(2) << 3.14159 << ' '
     << std::scientific << 1234.5;
  CHECK(fx.str() == "3.14 1.23e+03");

  std::istringstream is("123 -45 6.25 word 0x1f");
  int a = 0, b = 0, h = 0;
  double d = 0;
  std::string w;
  is >> a >> b >> d >> w >> std::hex >> h;
  CHECK(a == 123 && b == -45 && d == 6.25 && w == "word" && h == 31);
  int bad = 5;
  std::istringstream ns("nope");
  ns >> bad;
  CHECK(ns.fail() && bad == 0);

  std::stringstream lines("first line\nsecond\n");
  std::string l1, l2;
  std::getline(lines, l1);
  std::getline(lines, l2);
  CHECK(l1 == "first line" && l2 == "second");

  {
    std::ofstream out("IOS.TXT");
    CHECK(out.is_open());
    out << "line one\n" << 99 << '\n';
  }
  {
    std::ifstream in("IOS.TXT");
    std::string t;
    int n = 0;
    std::getline(in, t);
    in >> n;
    CHECK(t == "line one" && n == 99);
  }
  {
    // Binary files keep LF; text files have CR LF on disk.
    std::ifstream in("IOS.TXT", std::ios::binary);
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(raw == "line one\r\n99\r\n");
  }
  std::remove("IOS.TXT");

  std::string in1, in2;
  std::getline(std::cin, in1);
  std::getline(std::cin, in2);
  CHECK(in1 == "first" && in2 == "second line");
  std::cerr << "to stderr\n";
  std::clog << "to clog\n";

  std::cout << (fails ? "FAILED" : "ALL PASSED") << '\n';
  return fails;
}
