/* C++ exceptions: SjLj function contexts, the unwinder in libunwind.a and
 * libc++abi's personality. Built with -fexceptions. Prints ALL PASSED. */
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int live;  /* Tracker objects alive */
static int order[8], norder;
struct Tracker {
  int id;
  explicit Tracker(int i) : id(i) { live++; }
  ~Tracker() { live--; if (norder < 8) order[norder++] = id; }
};

struct Base { int code; explicit Base(int c) : code(c) {} virtual ~Base() {} };
struct Derived : Base { long extra; Derived(int c, long e) : Base(c), extra(e) {} };

__attribute__((noinline)) static void throw_int(int v) { if (v) throw v; }
__attribute__((noinline)) static void throw_derived(int depth) {
  Tracker t(depth);
  if (depth == 0)
    throw Derived(7, 123456L);
  throw_derived(depth - 1);
}
__attribute__((noinline)) static int no_handler(int v) {
  /* A frame with no landing pad of its own. */
  throw_int(v);
  return v;
}

struct Thrower {
  Tracker a{1};
  Tracker b;
  explicit Thrower(bool fail) : b(2) { if (fail) throw std::runtime_error("ctor"); }
};

static int rethrown() {
  try {
    try {
      throw_int(5);
    } catch (int &e) {
      e += 10;
      throw;
    }
  } catch (int e) {
    return e;
  }
  return -1;
}

int main() {
  /* Catch by value, and code after the try runs. */
  int got = 0;
  try { throw_int(42); } catch (int e) { got = e; }
  CHECK(got == 42);

  /* No exception: the handler does not run. */
  got = 1;
  try { throw_int(0); } catch (...) { got = 2; }
  CHECK(got == 1);

  /* Through frames without handlers, and destructors on the way. */
  live = 0; norder = 0;
  try {
    Tracker outer(100);
    throw_derived(3);
  } catch (Base &b) {
    CHECK(b.code == 7);
    CHECK(static_cast<Derived &>(b).extra == 123456L);
    CHECK(live == 0);
  }
  CHECK(norder == 5 && order[0] == 0 && order[3] == 3 && order[4] == 100);

  /* catch (...) and a type that does not match first. */
  got = 0;
  try { no_handler(9); } catch (const char *) { got = -1; } catch (...) { got = 9; }
  CHECK(got == 9);

  /* Rethrow with throw;, changing the object. */
  CHECK(rethrown() == 15);

  /* A constructor that throws destroys the members built so far. */
  live = 0; norder = 0;
  try { Thrower t(true); } catch (const std::exception &e) {
    CHECK(std::strcmp(e.what(), "ctor") == 0);
  }
  CHECK(live == 0 && norder == 2 && order[0] == 2 && order[1] == 1);

  /* libc++ throws: vector::at, string, bad_alloc. */
  std::vector<int> v(3);
  got = 0;
  try { v.at(5) = 1; } catch (const std::out_of_range &) { got = 1; }
  CHECK(got == 1);
  got = 0;
  try { std::string s("abc"); (void)s.substr(10); } catch (const std::logic_error &) { got = 1; }
  CHECK(got == 1);
  got = 0;
  {
    /* More than all of conventional memory, freed on the way out. */
    std::unique_ptr<char[]> blocks[30];
    try {
      for (auto &b : blocks)
        b.reset(new char[40000]);
    } catch (const std::bad_alloc &) { got = 1; }
  }
  CHECK(got == 1);

  /* exception_ptr and uncaught_exceptions. */
  std::exception_ptr ep;
  try { throw std::runtime_error("saved"); } catch (...) { ep = std::current_exception(); }
  got = 0;
  try { std::rethrow_exception(ep); } catch (const std::runtime_error &e) {
    got = std::strcmp(e.what(), "saved") == 0;
  }
  CHECK(got == 1);
  CHECK(std::uncaught_exceptions() == 0);

  /* Many throws: no leak of contexts or memory. */
  int sum = 0;
  for (int i = 1; i <= 200; ++i)
    try { throw_int(i); } catch (int e) { sum += e; }
  CHECK(sum == 20100);

  if (fails == 0)
    std::printf("ALL PASSED\n");
  return fails != 0;
}
