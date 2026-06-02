#include "text.hpp"
#include <algorithm>
#include <cctype>

char const* TimeToString(int sec) {
  static char ret[6];

  ret[0] = '0' + (sec / 600);
  ret[1] = '0' + (sec % 600) / 60;
  ret[2] = ':';
  ret[3] = '0' + (sec % 60) / 10;
  ret[4] = '0' + (sec % 10);
  ret[5] = 0;

  return ret;
}

char const* TimeToStringEx(int ms, bool force_hours, bool force_minutes) {
  static char ret[10];

  int c = 0;
  if (ms >= 6000000 || force_hours) {
    ret[c++] = '0' + (ms / 6000000);
    ms %= 6000000;
  }

  if (ms >= 60000 || force_minutes) {
    ret[c++] = '0' + (ms / 600000);
    ms %= 600000;
    ret[c++] = '0' + (ms / 60000);
    ms %= 60000;
    ret[c++] = ':';
  }
  ret[c++] = '0' + ms / 10000;
  ms %= 10000;
  ret[c++] = '0' + ms / 1000;
  ms %= 1000;
  ret[c++] = '.';
  ret[c++] = '0' + ms / 100;
  ms %= 100;
  ret[c++] = '0' + ms / 10;
  ret[c++] = 0;

  return ret;
}

char const* TimeToStringFrames(int frames) { return TimeToStringEx(frames * 14, false, false); }

int SafeToUpper(char ch) { return std::toupper(static_cast<unsigned char>(ch)); }

bool CiCompare(std::string const& a, std::string const& b) {
  if (a.size() != b.size()) return false;

  for (std::size_t i = 0; i < a.size(); ++i) {
    if (SafeToUpper(a[i]) != SafeToUpper(b[i])) return false;
  }

  return true;
}

bool CiStartsWith(std::string const& text, std::string const& starts_with) {
  if (starts_with.size() > text.size()) return false;

  for (std::size_t i = 0; i < starts_with.size(); ++i) {
    if (SafeToUpper(text[i]) != SafeToUpper(starts_with[i])) return false;
  }

  return true;
}

bool CiLess(std::string const& a, std::string const& b) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (i >= b.size())  // a is longer, thus a > b
      return false;
    int ach = SafeToUpper(a[i]);
    int bch = SafeToUpper(b[i]);
    if (ach < bch)
      return true;
    else if (ach > bch)
      return false;
  }

  return b.size() > a.size();  // if b is longer, then a < b, otherwise a == b
}

char Utf8ToDos(const char* str) {
  if (str[1] == 0) {
    return str[0];
  }
  char table[][3] = {
      {(char)0xc3, (char)0xa5, (char)0x86},  // å
      {(char)0xc3, (char)0xa4, (char)0x84},  // ä
      {(char)0xc3, (char)0xb6, (char)0x94},  // ö
      {(char)0xc3, (char)0x85, (char)0x8f},  // Å
      {(char)0xc3, (char)0x84, (char)0x8e},  // Ä
      {(char)0xc3, (char)0x96, (char)0x99},  // Ö
  };

  for (std::size_t i = 0; i < sizeof(table) / sizeof(*table); ++i) {
    if (table[i][0] == str[0] && table[i][1] == str[1]) return table[i][2];
  }
  return '?';
}
