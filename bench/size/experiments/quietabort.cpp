// Size experiment: libc++'s and libc++abi's abort messages without vfprintf.
//
// std::__libcpp_verbose_abort and __abort_message format their message with
// vfprintf, so every C++ program that can reach an abort path (operator
// new, string and vector length checks, guard variables, the iostreams
// initializer) links the whole floating-point printf: about 13 KiB of code
// in the small model. These replacements write the format string as is.
// run.sh links this file into the "clang-tuned" builds.
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void message(const char *f) {
  write(2, f, strlen(f));
  write(2, "\r\n", 2);
}

extern "C" [[noreturn]] void __abort_message(const char *f, ...) {
  message(f);
  abort();
}

namespace std {
inline namespace __1 {
[[noreturn]] void __libcpp_verbose_abort(const char *f, ...) noexcept {
  message(f);
  abort();
}
} // namespace __1
} // namespace std
