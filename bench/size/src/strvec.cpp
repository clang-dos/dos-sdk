// std::string and std::vector: building, appending, sorting, searching.
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

int main() {
  std::vector<std::string> words;
  const char *src[] = {"pear", "apple", "fig", "banana", "cherry"};
  for (unsigned i = 0; i < sizeof src / sizeof *src; ++i)
    words.push_back(src[i]);
  std::sort(words.begin(), words.end());

  std::string joined;
  for (unsigned i = 0; i < words.size(); ++i) {
    if (i) joined += ", ";
    joined += words[i];
  }
  std::vector<int> v;
  for (int i = 0; i < 100; ++i) v.push_back((i * 37) % 101);
  std::sort(v.begin(), v.end());
  long sum = 0;
  for (unsigned i = 0; i < v.size(); ++i) sum += v[i];

  std::string::size_type pos = joined.find("fig");
  printf("%s\n%u %ld %d\n", joined.c_str(), (unsigned)pos, sum, v[50]);
  return 0;
}
