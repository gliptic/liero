#include "reader.hpp"
#include "filesystem.hpp"
#include <map>
#include <stdexcept>
#include <vector>

std::string configRoot;

void setConfigPath(std::string const& path)
{	
	configRoot = path;
}
