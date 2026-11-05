// std::atomic on any CPU: on an 8086, 286 or 386, which have no XADD or
// CMPXCHG, atomic writes are calls to compiler-rt's __sync_* functions.
#include <atomic>
#include <cstdio>

static std::atomic<int> i{40};
static std::atomic<long> l{100000L};
static std::atomic<char> c{'a'};
static std::atomic<unsigned short> u{7};

// An order the compiler can't see.
static std::memory_order order(int n) {
  return n ? std::memory_order_acquire : std::memory_order_seq_cst;
}

int main(int argc, char **) {
  int fails = 0;
  i.fetch_add(3);
  i -= 1;
  int e = 42;
  if (!i.compare_exchange_strong(e, 42, order(argc), order(argc - 1)))
    fails++;
  e = 1;
  if (i.compare_exchange_strong(e, 0) || e != 42)
    fails++;
  l.fetch_add(23456789L);
  if (l.load() != 23556789L)
    fails++;
  long expected = 23556789L;
  if (!l.compare_exchange_weak(expected, 123456789L) &&
      !l.compare_exchange_strong(expected, 123456789L))
    fails++;
  if (c.exchange('b') != 'a' || c.load() != 'b')
    fails++;
  u.fetch_or(8);
  u &= 13;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (u.load(order(argc)) != 13)
    fails++;
  std::printf("atomics %s %d %ld\n", fails ? "FAILED" : "ok", i.load(),
              l.load());
  return fails;
}
