/* The C++ name demangler, __cxa_demangle, in every model but compact.
 * Built with -frtti for typeid names. Prints ALL PASSED. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <typeinfo>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

namespace ns { template <class T> struct Box { T t; }; }

static bool demangles(const char *mangled, const char *want) {
  int status = -1;
  char *s = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  bool ok = status == 0 && s && std::strcmp(s, want) == 0;
  if (!ok)
    std::printf("  %s -> %s (status %d), want %s\n", mangled, s ? s : "(null)",
                status, want);
  std::free(s);
  return ok;
}

int main() {
  CHECK(demangles("_Z3fooi", "foo(int)"));
  CHECK(demangles("_ZN2ns3BoxIiE3getEv", "ns::Box<int>::get()"));
  CHECK(demangles("_ZNSt3__16vectorIiNS_9allocatorIiEEE9push_backERKi",
                  "std::__1::vector<int, std::__1::allocator<int>>::"
                  "push_back(int const&)"));
  CHECK(demangles("_ZZ4mainE1x", "main::x"));
  CHECK(demangles("_ZTVN10__cxxabiv117__class_type_infoE",
                  "vtable for __cxxabiv1::__class_type_info"));
  /* A type name, as typeid gives it. */
  CHECK(demangles(typeid(ns::Box<long>).name(), "ns::Box<long>"));

  /* An invalid name, and a caller's buffer that has to grow. */
  int status = 0;
  CHECK(abi::__cxa_demangle("not mangled", nullptr, nullptr, &status) ==
        nullptr);
  CHECK(status == -2);
  size_t n = 4;
  char *buf = static_cast<char *>(std::malloc(n));
  buf = abi::__cxa_demangle("_Z3barPKcz", buf, &n, &status);
  CHECK(status == 0 && buf && std::strcmp(buf, "bar(char const*, ...)") == 0);
  CHECK(n >= std::strlen("bar(char const*, ...)") + 1);
  std::free(buf);

  if (fails == 0)
    std::printf("ALL PASSED\n");
  return fails != 0;
}
