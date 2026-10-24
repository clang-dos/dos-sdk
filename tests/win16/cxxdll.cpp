/* A C++ DLL. Objects on the stack work in every model: in the small and
 * medium models, the compiler keeps those whose address escapes (`this`
 * included) in DGROUP. */
#include <algorithm>
#include <string>
#include <vector>

#define API __export __far __pascal

namespace {
class Counter {
  int n = 0;
  std::string log;

public:
  __attribute__((noinline)) void add(int v) {
    n += v;
    log += std::to_string(v);
    log += ',';
  }
  __attribute__((noinline)) int total() const { return n; }
  const std::string &text() const { return log; }
};
} // namespace

/* Returns 0 when all of it works. */
extern "C" int API CxxWork(int n) {
  int bad = 0;
  Counter c;
  for (int i = 1; i <= n; i++)
    c.add(i);
  if (c.total() != n * (n + 1) / 2)
    bad |= 1;
  if (c.text() != "1,2,3,4,")
    bad |= 2;
  std::vector<std::string> words{"pear", "apple", "fig"};
  std::sort(words.begin(), words.end());
  if (words[0] != "apple" || words[2] != "pear")
    bad |= 4;
  std::string s = "x";
  for (int i = 0; i < 5; i++)
    s = s + s;
  if (s.size() != 32)
    bad |= 8;
  return bad;
}
