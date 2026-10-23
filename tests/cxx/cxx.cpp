#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/* C++ on DOS with libc++: virtual functions, containers, strings, smart
 * pointers, std::function, statics and global constructors. Prints
 * ALL PASSED. */
struct Shape { virtual ~Shape() = default; virtual int area() const = 0; };
struct Sq : Shape { int s; explicit Sq(int s) : s(s) {} int area() const override { return s * s; } };
struct Rect : Shape { int w, h; Rect(int w, int h) : w(w), h(h) {} int area() const override { return w * h; } };

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)

struct G { int v = 42; G() { std::puts("global ctor"); } ~G() { std::puts("global dtor"); } } g;
static int counter() { static int n = std::printf("static init\n"); return ++n; }

int main() {
  CHECK(g.v == 42);
  std::vector<std::unique_ptr<Shape>> shapes;
  shapes.push_back(std::make_unique<Sq>(3));
  shapes.push_back(std::make_unique<Rect>(2, 5));
  int total = 0;
  for (auto &s : shapes) total += s->area();
  CHECK(total == 19);

  std::vector<int> v{5, 2, 9, 1, 7};
  std::sort(v.begin(), v.end());
  CHECK(v.front() == 1 && v.back() == 9);
  v.resize(1000, 3);
  CHECK(v.size() == 1000 && v[999] == 3);

  std::string s = "hello";
  s += ", DOS";
  s.append(30, '!');
  CHECK(s.size() == 40 && s.substr(0, 10) == "hello, DOS");
  CHECK(std::to_string(-12345) == "-12345");
  CHECK(std::stoi("777") == 777);

  std::map<std::string, int> m;
  m["one"] = 1; m["two"] = 2; m["three"] = 3;
  CHECK(m.size() == 3 && m["two"] == 2);

  auto sp = std::make_shared<int>(7);
  auto sp2 = sp;
  CHECK(sp.use_count() == 2 && *sp2 == 7);

  int base = 10;
  std::function<int(int)> add = [base](int x) { return x + base; };
  CHECK(add(5) == 15);

  int a = counter(), b = counter();
  CHECK(b == a + 1);
  std::printf("%s\n", s.c_str());
  std::puts(fails ? "FAILED" : "ALL PASSED");
  return fails;
}
