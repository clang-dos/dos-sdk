#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

/* std::ios_base::sync_with_stdio: switching to unsynchronized streams and
 * back keeps what the streams buffered. What cin read ahead from standard
 * input is given back to stdin, and output written to cout while it was
 * unsynchronized comes out before what printf writes after the switch back.
 * Reads two lines from standard input. Prints
 *   order abbbcddee42f ALL PASSED
 * or, with -DWIDE, which also switches wcout,
 *   order abbbcddee42fgg7h ALL PASSED
 * so the caller checks the whole line. */
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
  /* cin reads ahead into its own buffer while it is unsynchronized; the
     second line must still be there for stdin afterwards. */
  CHECK(std::ios_base::sync_with_stdio(false));
  std::string line;
  std::getline(std::cin, line);
  CHECK(line == "first");
  CHECK(!std::ios_base::sync_with_stdio(true));
  char buf[32] = "";
  CHECK(std::fgets(buf, sizeof buf, stdin) != 0);
  CHECK(std::strcmp(buf, "second line\n") == 0);

  /* The first character written to an unsynchronized stream goes straight
     to stdout; the ones after it wait in the stream's buffer. */
  std::cout << "order a";
  std::ios_base::sync_with_stdio(false);
  std::cout << "bb" << 'b';
  std::ios_base::sync_with_stdio(true);
  std::printf("c");
  std::cout << "dd";
  std::ios_base::sync_with_stdio(false);
  std::cout << "ee" << 42;
  std::ios_base::sync_with_stdio(true);
  std::fputs("f", stdout);
#ifdef WIDE
  std::ios_base::sync_with_stdio(false);
  std::wcout << L"gg" << 7;
  std::ios_base::sync_with_stdio(true);
  std::printf("h");
#endif

  std::cout << (fails ? " FAILED" : " ALL PASSED") << std::endl;
  return fails;
}
