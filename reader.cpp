#include "reader.hpp"
#include "filesystem.hpp"
#include <map>
#include <stdexcept>
#include <vector>

std::string lieroEXERoot;

namespace
{

typedef std::map<std::string, ReaderFile> ReaderFileMap;

std::string lieroEXE;/*
std::string lieroCHR;
std::string lieroSND;*/

ReaderFileMap readerFiles;

}

void openFileUncached(ReaderFile& rf, std::string const& name)
{
	FILE* f = tolerantFOpen(name.c_str(), "rb");
	if(!f)
		throw std::runtime_error("Could not open '" + name + '\'');
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	rf.data = new uint8_t[len];
	rf.len = len;
	fread(rf.data, 1, len, f);
	fclose(f);
}

ReaderFile& openFile(std::string const& name)
{
	ReaderFileMap::iterator i = readerFiles.find(name);
	if(i != readerFiles.end())
	{
		i->second.seekg(0);
		return i->second;
	}

	ReaderFile& rf = readerFiles[name];
	openFileUncached(rf, name);

	return rf;
}

ReaderFile& openLieroEXE()
{
	return openFile(lieroEXE);
}

ReaderFile& openLieroSND()
{
	return openFile(changeLeaf(lieroEXE, "LIERO.SND"));
}

ReaderFile& openLieroCHR()
{
	return openFile(changeLeaf(lieroEXE, "LIERO.CHR"));
}

void setLieroEXE(std::string const& path)
{
	//TODO: Close cached files
	
	lieroEXE = path;
	/*
	lieroCHR = changeLeaf(path, "LIERO.CHR");
	lieroSND = changeLeaf(path, "LIERO.SND");*/
	
	lieroEXERoot = getRoot(lieroEXE);
}
