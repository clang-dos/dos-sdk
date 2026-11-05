// C++ with libc++, libc++abi and libunwind on PowerPC Mac OS X 10.4 and
// later: virtual calls, dynamic_cast, exceptions thrown through frames,
// containers and strings.
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static int fails;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      std::printf("FAIL line %d: %s\n", __LINE__, #c);                         \
      fails++;                                                                 \
    }                                                                          \
  } while (0)

struct Base {
  virtual ~Base() {}
  virtual int id() const { return 1; }
};
struct Derived : Base {
  int id() const override { return 2; }
};

static int destroyed;
struct Guard {
  ~Guard() { destroyed++; }
};

__attribute__((noinline)) static int thrower(int x) {
  Guard g; // destroyed while unwinding
  if (x > 2)
    throw std::runtime_error("too big");
  return x;
}

int main() {
  std::unique_ptr<Base> b(new Derived);
  CHECK(b->id() == 2);
  CHECK(dynamic_cast<Derived *>(b.get()) != nullptr);

  try {
    thrower(5);
    CHECK(!"no exception");
  } catch (const std::exception &e) {
    CHECK(std::string(e.what()) == "too big");
  }
  CHECK(destroyed == 1);
  try {
    throw 42;
  } catch (int i) {
    CHECK(i == 42);
  }

  std::vector<std::string> v;
  for (int i = 0; i < 100; i++)
    v.push_back(std::to_string(i));
  CHECK(v.size() == 100 && v[42] == "42");
  std::map<std::string, int> m;
  m["b"] = 2;
  m["a"] = 1;
  CHECK(m.begin()->first == "a" && m.size() == 2);
  try {
    (void)v.at(1000);
    CHECK(!"no exception");
  } catch (const std::out_of_range &) {
  }

  std::printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails != 0;
}
