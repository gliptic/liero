#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

// Tiny text formatter used by the stats screen to collect a row of fields
// with left / center / right placement.

struct TextCell {
  enum Placement { kLeft, kCenter, kRight };

  TextCell() = default;
  explicit TextCell(Placement p) : placement(p) {}

  // Streams use stringstream under the hood so any type with operator<<
  // works (ints, doubles, strings, char const*).
  template <typename T>
  TextCell& operator<<(T const& v) {
    std::ostringstream oss;
    oss << v;
    std::string s = oss.str();
    buffer.insert(buffer.end(), s.begin(), s.end());
    return *this;
  }

  // Convenience for the `cell().ref() << ...` pattern.
  TextCell& Ref() { return *this; }

  std::vector<uint8_t> buffer;
  Placement placement = kLeft;
};
