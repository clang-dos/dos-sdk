// Virtual classes and templates: a class hierarchy called through base
// pointers, and a class template and function template used with two types.
#include <stdio.h>

struct Shape {
  virtual ~Shape() {}
  virtual long area() const = 0;
  virtual const char *name() const = 0;
};
struct Rect : Shape {
  int w, h;
  Rect(int w, int h) : w(w), h(h) {}
  long area() const { return (long)w * h; }
  const char *name() const { return "rect"; }
};
struct Square : Rect {
  Square(int s) : Rect(s, s) {}
  const char *name() const { return "square"; }
};
struct Tri : Shape {
  int b, h;
  Tri(int b, int h) : b(b), h(h) {}
  long area() const { return (long)b * h / 2; }
  const char *name() const { return "tri"; }
};

template <class T, int N> class Stack {
  T items[N];
  int n;
public:
  Stack() : n(0) {}
  bool push(const T &t) { if (n == N) return false; items[n++] = t; return true; }
  T pop() { return items[--n]; }
  bool empty() const { return n == 0; }
};

template <class T> T maxOf(const T *a, int n) {
  T m = a[0];
  for (int i = 1; i < n; ++i) if (m < a[i]) m = a[i];
  return m;
}

int main() {
  Shape *s[3];
  s[0] = new Rect(3, 4);
  s[1] = new Square(5);
  s[2] = new Tri(6, 7);
  Stack<long, 8> areas;
  for (int i = 0; i < 3; ++i) {
    printf("%s %ld\n", s[i]->name(), s[i]->area());
    areas.push(s[i]->area());
  }
  Stack<const char *, 4> names;
  names.push("a"); names.push("b");
  long a[3]; int k = 0;
  while (!areas.empty()) a[k++] = areas.pop();
  unsigned d[3] = {15, 25, 5};
  printf("%ld %s %d\n", maxOf(a, 3), names.pop(), (int)maxOf(d, 3));
  for (int i = 0; i < 3; ++i) delete s[i];
  return 0;
}
