#include "common_writer.hpp"
#include "common_exereader.hpp"
#include "game/filesystem.hpp"
#include "game/reader.hpp"

int main(int argc, char *argv[])
{
	std::string configPath; // Default to current dir
	std::string exePath, tcName;

	for(int i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1])
			{
			case '-':
				if (std::strcmp(argv[i] + 2, "config-root") == 0 && i + 1 < argc)
				{
					++i;
					configPath = argv[i];
				}
				else if (std::strcmp(argv[i] + 2, "tc-name") == 0 && i + 1 < argc)
				{
					++i;
					tcName = argv[i];
				}
				break;
			}
		}
		else
		{
			exePath = argv[i];
		}
	}

	if (exePath.empty())
	{
		printf("tctool <path-to-tc>\n");
		return 0;
	}

	Common common;

	FsNode path(argv[1]);

	bool found = false;

	for (auto const& name : path.iter())
	{
		if (toUpperCase(name.name).find(".EXE") != std::string::npos)
		{
			ReaderFile exe((path / name.name).toSource());

			if (exe.len >= 135000 && exe.len <= 137000)
			{
				printf("Converting %s...\n", name.name.c_str());

				// TODO: Some TCs change the name of the .SND or .CHR for some reason.
				// We could read that name from the exe to make them work.
				ReaderFile gfx((path / "LIERO.CHR").toSource());
				ReaderFile snd((path / "LIERO.SND").toSource());

				loadFromExe(common, exe, gfx, snd);

				if (tcName.empty())
					tcName = getLeaf(argv[1]);

				auto writePath = joinPath(joinPath(configPath, "TC"), tcName);

				printf("Writing to %s...\n", writePath.c_str());

				commonSave(common, writePath);

				found = true;
				break;
			}
		}
	}

	if (!found)
	{
		printf("Could not find a suitable LIERO.EXE in %s\n", argv[1]);
	}

	return 0;
}