#include "settings.hpp"

#include "reader.hpp"
#include "keys.hpp"
#include "gfx.hpp"
#include "filesystem.hpp"

#include <gvl/io/fstream.hpp>
#include <gvl/serialization/context.hpp>
#include <gvl/serialization/archive.hpp>

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
, scaleFilter(Settings::SfNearest)
, bloodParticleMax(700)
, fullscreenW(640)
, fullscreenH(480)
, aiFrames(70*2), aiMutations(2)
, zoneTimeout(30)
, selectBotWeapons(true)
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
		}
		
		for(int j = 0; j < 3; ++j)
		{
			wormSettings[i]->rgb[j] = defRGB[i][j];
		}
	}
}

bool Settings::load(std::string const& path, Rand& rand)
{
	FILE* opt = tolerantFOpen(path.c_str(), "rb");
	
	if(!opt)
		return false;
		
	std::size_t size = fileLength(opt);
	
	if(size < 155)
		return false; // .dat is too short
	
	gvl::octet_stream_reader reader(gvl::stream_ptr(new gvl::fstream(opt)));
	gvl::default_serialization_context context;
	
	archive_liero(gvl::in_archive<gvl::default_serialization_context>(reader, context), *this, rand);
	
	
	return true;
}

gvl::gash::value_type& Settings::updateHash()
{
	gvl::default_serialization_context context;
	gvl::hash_accumulator<gvl::gash> ha;
	
	archive(gvl::out_archive<gvl::default_serialization_context, gvl::hash_accumulator<gvl::gash> >(ha, context), *this);
	
	ha.flush();
	hash = ha.final();
	return hash;
}

void Settings::save(std::string const& path, Rand& rand)
{
	FILE* opt = fopen(path.c_str(), "wb");
	gvl::octet_stream_writer writer(gvl::stream_ptr(new gvl::fstream(opt)));

	gvl::default_serialization_context context;
	
	archive_liero(gvl::out_archive<gvl::default_serialization_context>(writer, context), *this, rand);
}

void Settings::generateName(WormSettings& ws, Rand& rand)
{
	ReaderFile f;

	try
	{
		openFileUncached(f, joinPath(lieroEXERoot, "NAMES.DAT"));
	}
	catch (std::runtime_error)
	{
		return;
	}
	
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
		ws.name = names[rand(Uint32(names.size()))];
		ws.randomName = true;
	}
}
