#include "console.hpp"
#include <cstdio>

namespace Console {
void write(std::string const& str) { std::fputs(str.c_str(), stdout); }

void writeLine(std::string const& str) {
  std::fputs(str.c_str(), stdout);
  std::fputs("\n", stdout);
}

void writeWarning(std::string const& str) {
  Console::write("WARNING: ");
  Console::writeLine(str);
}
}  // namespace Console