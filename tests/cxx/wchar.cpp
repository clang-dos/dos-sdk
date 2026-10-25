#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <locale>
#include <sstream>
#include <string>

/* Wide characters and locales on DOS: wstring, wide string streams,
 * <cwchar>, <cwctype>, the "C" locale's facets and std::locale. Prints
 * ALL PASSED. */
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
  std::wstring ws = L"wide";
  ws += L" string";
  CHECK(ws.size() == 11 && ws.find(L"str") == 5);
  CHECK(std::wcslen(L"abc") == 3 && std::wcscmp(L"a", L"b") < 0);
  CHECK(std::towupper(L'q') == L'Q' && std::iswdigit(L'7'));
  CHECK(std::to_wstring(-31) == L"-31" && std::stoi(L"88") == 88);

  std::wostringstream wos;
  wos << L"n=" << 1234 << L' ' << 2.5;
  CHECK(wos.str() == L"n=1234 2.5");
  std::wistringstream wis(L"17 wordy");
  int n = 0;
  std::wstring w;
  wis >> n >> w;
  CHECK(n == 17 && w == L"wordy");

  char mb[8];
  std::mbstate_t st{};
  CHECK(std::wcrtomb(mb, L'A', &st) == 1 && mb[0] == 'A');
  wchar_t wc = 0;
  CHECK(std::mbrtowc(&wc, "Z", 1, &st) == 1 && wc == L'Z');

  std::locale loc = std::locale::classic();
  CHECK(loc.name() == "C");
  CHECK(std::locale() == loc);
  CHECK(std::isalpha('x', loc) && !std::isdigit('x', loc));
  CHECK(std::toupper('m', loc) == 'M' && std::tolower(L'M', loc) == L'm');
  auto &ct = std::use_facet<std::ctype<wchar_t>>(loc);
  CHECK(ct.widen('k') == L'k' && ct.narrow(L'k', '?') == 'k');
  auto &np = std::use_facet<std::numpunct<char>>(loc);
  CHECK(np.decimal_point() == '.');
  auto &coll = std::use_facet<std::collate<char>>(loc);
  std::string x = "apple", y = "banana";
  CHECK(coll.compare(x.data(), x.data() + x.size(), y.data(), y.data() + y.size()) < 0);
  CHECK(loc(x, y));

  // A user facet replaces numpunct in a new locale, which keeps the rest.
  // It has numpunct's id, so it is numpunct to has_facet and use_facet.
  struct comma : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
  };
  std::locale c2(loc, new comma);
  CHECK(std::use_facet<std::numpunct<char>>(c2).decimal_point() == ',');
  CHECK(std::use_facet<std::numpunct<char>>(loc).decimal_point() == '.');
  CHECK(std::has_facet<std::collate<char>>(c2));
  std::ostringstream os;
  os.imbue(c2);
  os << 1.5;
  CHECK(os.str() == "1,5");

  std::puts(fails ? "FAILED" : "ALL PASSED");
  return fails;
}
