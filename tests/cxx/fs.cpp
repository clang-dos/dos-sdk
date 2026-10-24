#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

/* <filesystem> on DOS: paths with drive letters and backslashes,
 * directories, iteration, copying, renaming, sizes, times, permissions and
 * removal. Prints ALL PASSED. */
namespace fs = std::filesystem;
static int fails;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
  fs::path p = "C:\\DOS\\README.TXT";
  CHECK(p.root_name() == "C:" && p.root_directory() == "\\");
  CHECK(p.is_absolute() && fs::path("\\DOS").is_relative());
  CHECK(p.filename() == "README.TXT" && p.stem() == "README" && p.extension() == ".TXT");
  CHECK(p.parent_path() == "C:\\DOS");
  CHECK(p.generic_string() == "C:/DOS/README.TXT");
  CHECK((fs::path("A") / "B" / "C.TXT").string() == "A\\B\\C.TXT");
  CHECK((fs::path("A/B") / "C").string() == "A/B\\C");
  CHECK(fs::path("A/B/../C").lexically_normal() == "A\\C");
  std::vector<std::string> parts;
  for (const auto &e : fs::path("C:/X\\Y")) parts.push_back(e.string());
  CHECK(parts.size() == 4 && parts[0] == "C:" && parts[1] == "/" && parts[3] == "Y");

  std::error_code ec;
  fs::remove_all("FSTEST", ec);
  CHECK(fs::create_directory("FSTEST"));
  CHECK(fs::is_directory("FSTEST") && fs::exists("FSTEST"));
  CHECK(fs::create_directories("FSTEST\\SUB\\DEEP"));
  {
    std::ofstream("FSTEST\\A.TXT", std::ios::binary) << "hello";
    std::ofstream("FSTEST\\SUB\\B.TXT", std::ios::binary) << "0123456789";
  }
  CHECK(fs::is_regular_file("FSTEST/A.TXT"));
  CHECK(fs::file_size("FSTEST\\A.TXT") == 5);
  CHECK(!fs::exists("FSTEST\\NONE.TXT"));
  fs::file_size("FSTEST\\NONE.TXT", ec);
  CHECK(ec == std::errc::no_such_file_or_directory);

  std::vector<std::string> names;
  for (auto &e : fs::directory_iterator("FSTEST"))
    names.push_back(e.path().filename().string());
  std::sort(names.begin(), names.end());
  CHECK(names.size() == 2 && names[0] == "A.TXT" && names[1] == "SUB");

  int files = 0, dirs = 0;
  for (auto &e : fs::recursive_directory_iterator("FSTEST")) {
    if (e.is_directory()) dirs++;
    else files++;
  }
  CHECK(files == 2 && dirs == 2);

  fs::copy_file("FSTEST\\A.TXT", "FSTEST\\C.TXT");
  CHECK(fs::file_size("FSTEST\\C.TXT") == 5);
  fs::rename("FSTEST\\C.TXT", "FSTEST\\D.TXT");
  CHECK(!fs::exists("FSTEST\\C.TXT") && fs::exists("FSTEST\\D.TXT"));
  fs::resize_file("FSTEST\\D.TXT", 2);
  CHECK(fs::file_size("FSTEST\\D.TXT") == 2);

#ifndef EMU2 // emu2 does not set file times or attributes.
  auto t = fs::last_write_time("FSTEST\\A.TXT");
  auto t2 = t - std::chrono::hours(24 * 400);
  fs::last_write_time("FSTEST\\A.TXT", t2);
  auto t3 = fs::last_write_time("FSTEST\\A.TXT");
  CHECK(t3 < t && t - t3 > std::chrono::hours(24 * 399));

  fs::permissions("FSTEST\\D.TXT", fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
                  fs::perm_options::remove);
  CHECK((fs::status("FSTEST\\D.TXT").permissions() & fs::perms::owner_write) == fs::perms::none);
  fs::permissions("FSTEST\\D.TXT", fs::perms::owner_write, fs::perm_options::add);
  CHECK((fs::status("FSTEST\\D.TXT").permissions() & fs::perms::owner_write) != fs::perms::none);
#endif

  fs::path cwd = fs::current_path();
  CHECK(cwd.is_absolute() && cwd.has_root_name());
  fs::current_path("FSTEST");
  CHECK(fs::exists("A.TXT") && fs::current_path() == cwd / "FSTEST");
  fs::current_path("..");
  CHECK(fs::current_path() == cwd);
  CHECK(fs::absolute("FSTEST") == cwd / "FSTEST");
  CHECK(fs::equivalent(fs::canonical("FSTEST\\SUB\\..\\A.TXT"), fs::canonical("FSTEST\\A.TXT")));
  auto sp = fs::space("FSTEST");
  CHECK(sp.capacity > 0 && sp.available <= sp.capacity);

  CHECK(fs::remove("FSTEST\\D.TXT") && !fs::exists("FSTEST\\D.TXT"));
  CHECK(fs::remove_all("FSTEST") == 5);
  CHECK(!fs::exists("FSTEST"));

  std::puts(fails ? "FAILED" : "ALL PASSED");
  return fails;
}
