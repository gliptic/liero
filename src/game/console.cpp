#include "console.hpp"
#include <cstdio>

namespace console {
// stdout write helpers; failures are surfaced (eventually) via fflush at the
// application level, not here. NOLINT the return-value checks.
void Write(std::string const& str) {
  std::fputs(str.c_str(), stdout);  // NOLINT(cert-err33-c)
}

void WriteLine(std::string const& str) {
  std::fputs(str.c_str(), stdout);  // NOLINT(cert-err33-c)
  std::fputs("\n", stdout);         // NOLINT(cert-err33-c)
}

void WriteWarning(std::string const& str) {
  console::Write("WARNING: ");
  console::WriteLine(str);
}
}  // namespace console