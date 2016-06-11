#ifndef LIERO_TEXT_HPP
#define LIERO_TEXT_HPP

#include <string>
#include <cstring>
#include <cstdio>

inline std::string toString(int v)
{
	char buf[20];
	std::sprintf(buf, "%d", v);
	return buf;
}

char const* timeToString(int sec);
char const* timeToStringEx(int ms, bool forceHours, bool forceMinutes);
char const* timeToStringFrames(int frames);

inline void rtrim(std::string& str)
{
	std::string::size_type e = str.find_last_not_of(" \t\r\n");
	if(e == std::string::npos)
		str.clear();
	else
		str.erase(e + 1);
}

inline void findReplace(std::string& str, std::string const& find, std::string const& replace)
{
	std::string::size_type p = str.find(find);
	if(p != std::string::npos)
		str.replace(p, find.size(), replace);
}

inline bool endsWith(std::string const& str, char const* end)
{
	auto pos = str.find(end);
	return pos != std::string::npos && pos + std::strlen(end) == str.size();
}

bool ciStartsWith(std::string const& a, std::string const& b);
bool ciCompare(std::string const& a, std::string const& b);
bool ciLess(std::string const& a, std::string const& b);
// converts an extremely limited subset of UTF-8 to extended ASCII
char utf8ToDOS(char* str);

#endif // LIERO_TEXT_HPP
