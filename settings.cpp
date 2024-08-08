#include "settings.hpp"

#include "keys.hpp"
#include "gfx.hpp"
#include "filesystem.hpp"

#include <gvl/io2/fstream.hpp>
#include <gvl/serialization/context.hpp>
#include <gvl/serialization/archive.hpp>
#include <gvl/serialization/toml.hpp>

#include <gvl/crypt/gash.hpp>

int const Settings::wormAnimTab[] =
{
	0,
	7,
	0,
	14
};

Extensions::Extensions()
: recordReplays(true)
, loadPowerlevelPalette(true)
, bloodParticleMax(700)
, aiFrames(70*2), aiMutations(2)
, aiTraces(false)
, aiParallels(3)
, fullscreenW(640)
, fullscreenH(480)
, zoneTimeout(30)
, selectBotWeapons(true)
, allowViewingSpawnPoint(false)
, singleScreenReplay(false)
, spectatorWindow(false)
{
}

Settings::Settings()
: maxBonuses(4)
, blood(100)
, timeToLose(600)
, flagsToWin(20)
, gameMode(0)
, shadow(true)
, loadChange(true)
, namesOnBonuses(false)
, regenerateLevel(false)
, lives(15)
, loadingTime(100)
, randomLevel(true)
, map(true)
, screenSync(true)
{
	std::memset(weapTable, 0, sizeof(weapTable));

	wormSettings[0].reset(new WormSettings);
	wormSettings[1].reset(new WormSettings);

	wormSettings[0]->color = 32;
	wormSettings[1]->color = 41;

	unsigned char defControls[2][7] =
	{
		{0x13, 0x21, 0x20, 0x22, 0x1D, 0x2A, 0x38},
		{0xA0, 0xA8, 0xA3, 0xA5, 0x75, 0x90, 0x36}
	};

	unsigned char defRGB[2][3] =
	{
		{26, 26, 63},
		{15, 43, 15}
	};

	for(int i = 0; i < 2; ++i)
	{
		for(int j = 0; j < 7; ++j)
		{
			wormSettings[i]->controls[j] = defControls[i][j];
			wormSettings[i]->controlsEx[j] = defControls[i][j];
		}

		for(int j = 0; j < 3; ++j)
		{
			wormSettings[i]->rgb[j] = defRGB[i][j];
		}
	}
}

typedef gvl::in_archive<gvl::octet_reader> in_archive_t;
typedef gvl::out_archive<gvl::octet_writer> out_archive_t;

bool Settings::load(FsNode node, Rand& rand)
{
	try
	{
		auto reader = node.toOctetReader();
		gvl::default_serialization_context context;

		gvl::toml::reader<gvl::octet_reader> ar(reader);

		archive_text(*this, ar);
	}
	catch (std::runtime_error&)
	{
		return false;
	}

	return true;
}

bool Settings::loadLegacy(FsNode node, Rand& rand)
{
	try
	{
		auto reader = node.toOctetReader();
		gvl::default_serialization_context context;

		archive_liero(in_archive_t(reader, context), *this, rand);
	}
	catch (std::runtime_error&)
	{
		return false;
	}

	return true;
}

gvl::gash::value_type& Settings::updateHash()
{
	gvl::default_serialization_context context;
	gvl::hash_accumulator<gvl::gash> ha;

	archive(gvl::out_archive<gvl::hash_accumulator<gvl::gash>, gvl::default_serialization_context>(ha, context), *this);

	ha.flush();
	hash = ha.final();
	return hash;
}

void Settings::save(FsNode node, Rand& rand)
{
	auto writer = node.toOctetWriter();

	gvl::toml::writer<gvl::octet_writer> ar(writer);

	archive_text(*this, ar);
}

void Settings::generateName(WormSettings& ws, Rand& rand)
{
#if 0 // TODO
	try
	{
		ReaderFile f(FsNode(joinPath(configRoot, "NAMES.DAT")).read());

		std::vector<std::string> names;

		std::size_t len = f.len;

		// TODO: This is a bit silly since we switched to ReaderFile
		std::vector<char> chars(len);

		f.get(reinterpret_cast<uint8_t*>(&chars[0]), len);

		std::size_t begin = 0;
		for(std::size_t i = 0; i < len; ++i)
		{
			if(chars[i] == '\r'
			|| chars[i] == '\n')
			{
				if(i > begin)
				{
					names.push_back(std::string(chars.begin() + begin, chars.begin() + i));
				}

				begin = i + 1;
			}
		}

		if(!names.empty())
		{
			ws.name = names[rand(uint32_t(names.size()))];
			ws.randomName = true;
		}
	}
	catch (std::runtime_error)
	{
		return;
	}
#endif
}
