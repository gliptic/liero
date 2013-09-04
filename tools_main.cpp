#ifndef UUID_8615289728154E2FB9B179C2745D5FA9
#define UUID_8615289728154E2FB9B179C2745D5FA9

#include "sys.hpp"
#include "reader.hpp"
#include "filesystem.hpp"
#include "text.hpp"
#include "constants.hpp"
#include "math.hpp"
#include "console.hpp"
#include <gvl/support/platform.hpp>

#include <ctime>
#include <exception>

#include "replay_to_video.hpp"

bool match(unsigned char const* str, unsigned char const* pat)
{
	if (*pat == '*') return match(str, pat + 1) || match(str + 1, pat);
	if (!*str) return !*pat;
	return (toupper(*str) == toupper(*pat) || *pat == '?') && match(str + 1, pat + 1);
}

bool match(std::string const& str, std::string const& pat)
{
	return match((unsigned char const*)str.c_str(), (unsigned char const*)pat.c_str());
}

int main(int argc, char *argv[])
try
{
	bool exeSet = false, dir = false;
	gvl::shared_ptr<Common> common(new Common);

	std::string replayPath;
	
	for(int i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1])
			{
			case 'd':
				dir = true;
				break;

			case 'r':
				++i;
				if (i < argc)
					replayPath = &argv[i][0];
			break;
			}
		}
		else
		{
			setLieroEXE(argv[i]);
			exeSet = true;
		}
	}
	
	if(!exeSet)
		setLieroEXE("LIERO.EXE");

	common->texts.loadFromEXE();

	//initKeys();
	common->loadConstantsFromEXE();
	loadTablesFromEXE();

	common->font.loadFromEXE();
	common->loadPalette();
	common->loadGfx();
	common->loadMaterials();
	common->loadWeapons();
	common->loadTextures();
	common->loadOthers();
	common->loadSfx();

	if (dir)
	{
		auto const& root = getRoot(replayPath);
		DirectoryIterator di(root);

		for (; di; ++di)
		{
			auto const& path = *di;
			if (getExtension(path) == "lrp")
			{
				auto const& fullPath = joinPath(root, path);
				if (match(fullPath, replayPath))
				{
					printf("Converting %s\n", fullPath.c_str());
					replayToVideo(common, fullPath, fullPath + ".mp4");
				}
			}
		}
	}
	else
	{
		replayToVideo(common, replayPath, replayPath + ".mp4");
	}

	return 0;
}
catch(std::exception& ex)
{
	Console::writeLine(std::string("EXCEPTION: ") + ex.what());
	//Console::writeLine("Press any key to quit");
	//Console::waitForAnyKey();
	return 1;
}

#endif // UUID_8615289728154E2FB9B179C2745D5FA9
