#include "console.hpp"
#include <cstdio>

namespace console {
void Write(std::string const& str) { std::fputs(str.c_str(), stdout); }

void WriteLine(std::string const& str) {
  std::fputs(str.c_str(), stdout);
  std::fputs("\n", stdout);
}

void WriteWarning(std::string const& str) {
  console::Write("WARNING: ");
  console::WriteLine(str);
}
}  // namespace console