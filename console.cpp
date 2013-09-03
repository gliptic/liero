#include "console.hpp"
#include <gvl/support/platform.hpp>
#include <cstdio>
//#include <iostream>

namespace Console
{

void write(std::string const& str)
{
	std::fputs(str.c_str(), stdout);
}

void writeLine(std::string const& str)
{
	std::fputs(str.c_str(), stdout);
	std::fputs("\n", stdout);
}

}

namespace Console
{

void writeWarning(std::string const& str)
{
	Console::write("WARNING: ");
	Console::writeLine(str);
}

}
