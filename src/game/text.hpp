#pragma once

#include <cstdio>
#include <cstring>
#include <string>

inline std::string ToString(int v) {
  const int kBufMax = 20;
  char buf[kBufMax];
  std::snprintf(buf, kBufMax * sizeof(char), "%d", v);
  return buf;
}

char const* TimeToString(int sec);
char const* TimeToStringEx(int ms, bool force_hours, bool force_minutes);
char const* TimeToStringFrames(int frames);

inline void Rtrim(std::string& str) {
  std::string::size_type e = str.find_last_not_of(" \t\r\n");
  if (e == std::string::npos)
    str.clear();
  else
    str.erase(e + 1);
}

inline void FindReplace(std::string& str, std::string const& find, std::string const& replace) {
  std::string::size_type p = str.find(find);
  if (p != std::string::npos) str.replace(p, find.size(), replace);
}

inline bool EndsWith(std::string const& str, char const* end) {
  auto pos = str.find(end);
  return pos != std::string::npos && pos + std::strlen(end) == str.size();
}

bool CiStartsWith(std::string const& a, std::string const& b);
bool CiCompare(std::string const& a, std::string const& b);
bool CiLess(std::string const& a, std::string const& b);
// converts an extremely limited subset of UTF-8 to extended ASCII
char Utf8ToDos(const char* str);
