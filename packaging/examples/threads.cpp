// C++ threads on Windows 2000/XP: std::thread, std::mutex, thread_local.
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

thread_local int tl = 0;

int main() {
  std::mutex m;
  long total = 0;
  std::vector<std::thread> ts;
  for (int i = 1; i <= 4; ++i)
    ts.emplace_back([&, i] {
      for (int k = 0; k < 1000; ++k)
        tl += i;
      std::lock_guard<std::mutex> l(m);
      total += tl;
    });
  for (auto &t : ts)
    t.join();
  std::cout << "threads total " << total << (total == 10000 ? " OK" : " WRONG") << std::endl;
  return total == 10000 ? 0 : 1;
}
