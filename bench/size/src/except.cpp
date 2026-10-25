// Exceptions: throw and catch by type and base class, unwinding destructors,
// rethrow and catch (...).
#include <stdio.h>
#include <stdexcept>

static int live = 0;
struct Guard {
  Guard() { ++live; }
  ~Guard() { --live; }
};

struct MyError : std::runtime_error {
  int code;
  MyError(int c) : std::runtime_error("my error"), code(c) {}
};

static int depth(int n) {
  Guard g;
  if (n == 0) throw MyError(42);
  return depth(n - 1) + 1;
}

static void rethrow() {
  try { depth(3); } catch (...) { throw; }
}

int main() {
  int caught = 0;
  try { depth(5); } catch (const MyError &e) { caught += e.code; }
  try { rethrow(); } catch (const std::exception &e) { caught += 1; }
  try { throw 7; } catch (int i) { caught += i; }
  printf("%d %d\n", caught, live);
  return 0;
}
