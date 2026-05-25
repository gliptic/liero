#include "settings.hpp"

#include "keys.hpp"
#include "gfx.hpp"
#include "filesystem.hpp"
#include "io/stream.hpp"

#include <serialization/cereal_types.hpp>
#include <serialization/toml_archive.hpp>

#include <xxhash.h>
#include <sstream>

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
, bonusTimeout(0)
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
		auto reader_ptr = node.toReader(); io::Reader& reader = *reader_ptr;
		std::string content;
		uint8_t buf[4096];
		for (;;) {
			std::size_t got = reader.try_get(buf, sizeof(buf));
			if (got == 0) break;
			content.append(reinterpret_cast<char*>(buf), got);
		}
		fromToml(content);
	}
	catch (std::runtime_error&)
	{
		return false;
	}

	// Validate that wormSettings were deserialized (guards against corrupt
	// or incompatible config files that lack player sections).
	for (int i = 0; i < NumWormSettings; ++i)
		if (!wormSettings[i])
			return false;

	return true;
}

uint64_t& Settings::updateHash()
{
	std::ostringstream ss;
	{
		cereal::TomlOutputArchive ar(ss);
		serializeGameplay(ar, *this);
	}
	std::string buf = ss.str();
	hash = XXH3_64bits(buf.data(), buf.size());
	return hash;
}

std::string Settings::toToml() const
{
	std::ostringstream ss;
	{
		cereal::TomlOutputArchive ar(ss);
		// Serialize all scalar fields under [settings]
		ar.setNextName("settings");
		ar.startNode();
		int32_t version = ConfigVersion;
		ar(cereal::make_nvp("version", version));
		serializeSettingsScalars(ar, const_cast<Settings&>(*this));
		serializeArray(ar, "weapTable", const_cast<Settings&>(*this).weapTable);
		ar.finishNode();

		// Serialize worm settings as sub-tables with descriptive names
		static const char* wormNames[] = {"player1", "player2", "network_player"};
		for (int i = 0; i < NumWormSettings; ++i) {
			if (wormSettings[i]) {
				ar.setNextName(wormNames[i]);
				ar.startNode();
				serializeWormSettingsToml(ar, *wormSettings[i]);
				ar.finishNode();
			}
		}
	}
	return ss.str();
}

void Settings::fromToml(std::string const& data)
{
	std::istringstream ss(data);
	cereal::TomlInputArchive ar(ss);

	ar.setNextName("settings");
	ar.startNode();
	int32_t version = 0;
	ar(cereal::make_nvp("version", version));
	serializeSettingsScalars(ar, *this);
	serializeArray(ar, "weapTable", weapTable);
	ar.finishNode();

	static const char* wormNames[] = {"player1", "player2", "network_player"};
	for (int i = 0; i < NumWormSettings; ++i) {
		ar.setNextName(wormNames[i]);
		ar.startNode();
		if (!wormSettings[i])
			wormSettings[i] = std::make_shared<WormSettings>();
		serializeWormSettingsToml(ar, *wormSettings[i]);
		ar.finishNode();
	}
}

void Settings::save(FsNode node, Rand& rand)
{
	auto writer_ptr = node.toWriter(); io::Writer& writer = *writer_ptr;
	std::string toml = toToml();
	writer.put(reinterpret_cast<uint8_t const*>(toml.data()), toml.size());
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
