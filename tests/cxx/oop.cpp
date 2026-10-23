/* C++ object model on DOS: virtual and multiple inheritance, thunks,
 * virtual bases, member function pointers, dynamic_cast and typeid. Built
 * with -frtti. Prints ALL PASSED. */
#include <cstdio>
#include <typeinfo>

static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

struct A { int a = 1; virtual ~A() {} virtual int f() const { return 10 + a; } int plain(int x) const { return x + a; } };
struct B { int b = 2; virtual ~B() {} virtual int g() const { return 20 + b; } };
struct C : A, B { int c = 3; int f() const override { return 30 + c; } int g() const override { return 40 + c; } };

struct V { int v = 5; virtual ~V() {} virtual int h() const { return v; } };
struct L : virtual V { int l = 6; int h() const override { return v + l; } };
struct R : virtual V { int r = 7; };
struct D : L, R { int d = 8; };

static int callg(const B &x) { return x.g(); }
static int viav(const V &x) { return x.h() * 100 + x.v; }

int main() {
  C c;
  A *pa = &c;
  B *pb = &c;
  CHECK(pa->f() == 33);
  CHECK(pb->g() == 43);          /* through a this-adjusting thunk */
  CHECK(callg(c) == 43);
  CHECK(dynamic_cast<C *>(pb) == &c);
  CHECK(dynamic_cast<A *>(pb) == pa);   /* cross cast */
  CHECK(dynamic_cast<void *>(pb) == static_cast<void *>(&c));
  CHECK(typeid(*pb) == typeid(C));
  CHECK(typeid(*pa).name()[0] != '\0');

  D d;
  V *pv = &d;
  CHECK(pv->h() == 11);
  CHECK(viav(d) == 1105);
  CHECK(static_cast<V &>(static_cast<R &>(d)).v == 5);
  CHECK(dynamic_cast<D *>(pv) == &d);
  CHECK(dynamic_cast<R *>(pv) == static_cast<R *>(&d));
  A a;
  CHECK(dynamic_cast<C *>(&a) == nullptr);
  bool caught = false;
  CHECK(typeid(a) != typeid(c));
  (void)caught;

  int (A::*pmf)() const = &A::f;     /* virtual */
  CHECK((a.*pmf)() == 11 && (c.*pmf)() == 33);
  int (A::*pmp)(int) const = &A::plain;
  CHECK((c.*pmp)(4) == 5);
  int (B::*pmg)() const = &B::g;
  int (C::*pmcg)() const = pmg;      /* base to derived: this adjustment */
  CHECK((c.*pmcg)() == 43);
  CHECK(pmf != nullptr && pmp != nullptr);
  int C::*pmd = &C::c;
  CHECK(c.*pmd == 3);

  std::printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails;
}
