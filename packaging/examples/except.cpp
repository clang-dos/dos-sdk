// C++ exceptions and RTTI (build with -fexceptions -frtti on i8086).
#include <cstdio>
#include <stdexcept>
#include <string>

struct Guard {
  int &n;
  ~Guard() { ++n; }
};

static void thrower(int &n) {
  Guard g{n};
  throw std::runtime_error("boom");
}

int main() {
  int n = 0;
  try {
    thrower(n);
  } catch (const std::exception &e) {
    std::printf("caught %s, %d destructor%s ran\n", e.what(), n, n == 1 ? "" : "s");
    return n == 1 ? 0 : 1;
  }
  return 1;
}
