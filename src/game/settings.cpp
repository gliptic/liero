#include "settings.hpp"

#include "keys.hpp"
#include "gfx.hpp"
#include "filesystem.hpp"

#include <gvl/io2/fstream.hpp>
#include <gvl/serialization/toml_adapter.hpp>

#include <gvl/crypt/gash.hpp>

int const Settings::wormAnimTab[] =
{
	0,
	7,
	0,
	14
};

GameplayExtensions::GameplayExtensions()
: recordReplays(true)
, loadPowerlevelPalette(true)
, aiFrames(70*2), aiMutations(2)
, aiTraces(false)
, aiParallels(3)
, zoneTimeout(30)
, selectBotWeapons(true)
, allowViewingSpawnPoint(false)
, tc(std::string("openliero"))
{
}

AppSettings::AppSettings()
: fullscreen(false)
, singleScreenReplay(false)
, spectatorWindow(false)
, bloodParticleMax(700)
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
	wormSettings[2].reset(new WormSettings);

	wormSettings[0]->color = 32;
	wormSettings[1]->color = 41;
	wormSettings[2]->color = 32;

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

	// Network player defaults to left player's controls and color
	for(int j = 0; j < 7; ++j)
	{
		wormSettings[2]->controls[j] = defControls[0][j];
		wormSettings[2]->controlsEx[j] = defControls[0][j];
	}
	for(int j = 0; j < 3; ++j)
	{
		wormSettings[2]->rgb[j] = defRGB[0][j];
	}
}

bool Settings::load(FsNode node, Rand& rand)
{
	try
	{
		auto reader = node.toOctetReader();
		gvl::toml::reader<gvl::octet_reader> ar(reader);
		archive_text(*this, ar);
	}
	catch (std::runtime_error&)
	{
		return false;
	}

	return true;
}

gvl::gash::value_type& Settings::updateHash()
{
	std::string buf;
	gvl::string_writer sw(buf);
	gvl::toml::writer<gvl::string_writer> ar(sw);
	archive_gameplay_text(*this, ar);
	ar.flush();

	gvl::hash_accumulator<gvl::gash> ha;
	for (char c : buf)
		ha.put(static_cast<uint8_t>(c));
	ha.flush();
	hash = ha.final();
	return hash;
}

std::string Settings::toToml() const
{
	std::string buf;
	gvl::string_writer sw(buf);
	gvl::toml::writer<gvl::string_writer> ar(sw);
	archive_text(const_cast<Settings&>(*this), ar);
	return buf;
}

void Settings::fromToml(std::string const& data)
{
	gvl::string_reader sr(data);
	gvl::toml::reader<gvl::string_reader> ar(sr);
	archive_text(*this, ar);
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
