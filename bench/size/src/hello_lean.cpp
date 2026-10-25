// hello_iostream with DOS_LEAN_AND_MEAN: char streams use the "C" locale's
// character rules without looking up ctype<char>. Other compilers ignore it.
#define DOS_LEAN_AND_MEAN
#include <iostream>
int main() { std::cout << "Hello, world!" << std::endl; return 0; }
