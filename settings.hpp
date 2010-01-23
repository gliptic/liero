#ifndef LIERO_SETTINGS_HPP
#define LIERO_SETTINGS_HPP

#include "worm.hpp"
#include <string>
#include <cstring>
#include <gvl/resman/shared_ptr.hpp>
#include <gvl/support/cstdint.hpp>
#include <gvl/crypt/gash.hpp>
#include <gvl/serialization/archive.hpp> // For gvl::enable_when
#include "version.hpp"

// We isolate extensions for the benefit of the .dat loader.
// It can then easily reset the extensions if they fail to load.
struct Extensions
{
	static int const myVersion = 2;
	
	Extensions();
	
	// Extensions
	bool extensions;
	bool recordReplays;
	bool loadPowerlevelPalette;
	uint32_t scaleFilter;
	bool depth32;
	int bloodParticleMax;
	
	int fullscreenW;
	int fullscreenH;
};

struct Settings : gvl::shared, Extensions
{
	enum
	{
		GMKillEmAll,
		GMGameOfTag,
		GMCtF,
		GMSimpleCtF
	};
	
	enum
	{
		SiGameMode,
		SiLives,
		SiTimeToLose, // Extra
		SiFlagsToWin, // Extra
		SiLoadingTimes,
		SiMaxBonuses,
		SiNamesOnBonuses,
		SiMap,
		SiAmountOfBlood,
		SiLevel,
		SiRegenerateLevel,
		SiShadows,
		SiScreenSync,
		SiLoadChange,
		SiPlayer1Options,
		SiPlayer2Options,
		SiWeaponOptions
	};
	
	enum
	{
		SfNearest,
		SfScale2X,
		
		SfMax
	};
	
	static int const selectableWeapons = 5;
	
	static int const wormAnimTab[];
	
	Settings();
	
	bool load(std::string const& path);
	void save(std::string const& path);
	gvl::gash::value_type& updateHash();
	
	static void generateName(WormSettings& ws);
	
	uint32_t weapTable[40];
	int maxBonuses;
	int blood;
	int timeToLose;
	int flagsToWin;
	uint32_t gameMode;
	bool shadow;
	bool loadChange;
	bool namesOnBonuses;
	bool regenerateLevel;
	int lives;
	int loadingTime;
	bool randomLevel;
	std::string levelFile;
	bool map;
	bool screenSync;
	
	gvl::shared_ptr<WormSettings> wormSettings[2];
	
	
	
	gvl::gash::value_type hash;
};

template<int L, int H>
inline int limit(int v)
{
	if(v >= H)
		return H - 1;
	else if(v < L)
		return L;
		
	return v;
}

template<typename Archive>
void archive_liero(Archive ar, Settings& settings)
{
	ar
	.ui8(settings.maxBonuses)
	.ui16_le(settings.loadingTime)
	.ui16_le(settings.lives)
	.ui16_le(settings.timeToLose)
	.ui16_le(settings.flagsToWin)	
	.b(settings.screenSync)
	.b(settings.map)
	.ui8(settings.wormSettings[0]->controller)
	.ui8(settings.wormSettings[1]->controller)
	.b(settings.randomLevel)
	.ui16_le(settings.blood)
	.ui8(settings.gameMode)
	.b(settings.namesOnBonuses)
	.b(settings.regenerateLevel)
	.b(settings.shadow);
	
	if(ar.in) settings.wormSettings[0]->controller &= 1;
	if(ar.in) settings.wormSettings[1]->controller &= 1;
	
	for(int i = 0; i < 40; ++i)
	{
		ar.ui8(settings.weapTable[i]);
		if(ar.in) settings.weapTable[i] = limit<0, 3>(settings.weapTable[i]);
	}
	
	for(int i = 0; i < 2; ++i)
	for(int j = 0; j < 3; ++j)
	{
		ar.ui8(settings.wormSettings[i]->rgb[j]);
		if(ar.in) settings.wormSettings[i]->rgb[j] &= 63;
	}
		
	for(int i = 0; i < 2; ++i)
	{
		for(int j = 0; j < 5; ++j)
		{
			ar.ui8(settings.wormSettings[i]->weapons[j]);
		}
	}

	ar.ui16_le(settings.wormSettings[0]->health);
	ar.ui16_le(settings.wormSettings[1]->health);

	for(int i = 0; i < 2; ++i)
	{
		if(settings.wormSettings[i]->randomName && ar.out)
		{
			std::string empty;
			ar.pascal_str(empty, 21);
		}
		else
		{
			ar.pascal_str(settings.wormSettings[i]->name, 21);
			if(ar.in)
			{
				if(settings.wormSettings[i]->name.empty())
					settings.generateName(*settings.wormSettings[i]);
				else
					settings.wormSettings[i]->randomName = false;
			}
		}
	}
	
	ar.b(settings.loadChange);
	
	char lieroStr[] = "\x05LIERO\0\0";
	for(std::size_t i = 0; i < sizeof(lieroStr); ++i)
	{
		ar.ui8(lieroStr[i]);
	}
	
	for(int i = 0; i < 2; ++i)
	{
		for(int j = 0; j < 7; ++j)
		{
			uint32_t v;
			if(ar.out)
			{
				v = settings.wormSettings[i]->controls[j];
				v = limit<0, 177>(v);
			}
			ar.ui8(v);
			if(ar.in)
			{
				v = limit<0, 177>(v);
				settings.wormSettings[i]->controls[j] = v;
			}
		}
	}
	
	ar.pascal_str(settings.levelFile, 9);

	// TODO: Slightly bad way to detect whether extensions exist, no?	
	try
	{
		// Extensions
		int fileExtensionVersion = myGameVersion;
		ar.ui8(fileExtensionVersion);
		
		ar.b(settings.extensions);
		ar.b(settings.recordReplays);
		ar.b(settings.loadPowerlevelPalette);
		ar.ui8(settings.scaleFilter);
		ar.ui16(settings.fullscreenW);
		ar.ui16(settings.fullscreenH);
		
		gvl::enable_when(ar, fileExtensionVersion >= 2)
			.b(settings.depth32, true);
			
		for(int i = 0; i < 2; ++i)
		{
			for(int c = 0; c < WormSettings::MaxControl; ++c)
			{
				int dummy = 0;
				gvl::enable_when(ar, fileExtensionVersion >= 2)
					.ui8(dummy, 255)
					.ui8(dummy, 255);
			}
		}
		
		for(int i = 0; i < 2; ++i)
		{
			WormSettings& ws = *settings.wormSettings[i];
			for(int c = 0; c < WormSettings::MaxControlEx; ++c)
			{
				gvl::enable_when(ar, fileExtensionVersion >= 3)
					.ui32(ws.controlsEx[c], ws.controls[c]);
			}
		}
	}
	catch(gvl::stream_error&)
	{
		// Reset to default state
		settings.Extensions::operator=(Extensions());
		
		for(int i = 0; i < 2; ++i)
		{
			WormSettings& ws = *settings.wormSettings[i];
			ws.WormSettingsExtensions::operator=(WormSettingsExtensions());
		}
	}
}


// Settings archiving, not including player (worm) settings
template<typename Archive>
void archive(Archive ar, Settings& settings)
{
	for(int i = 0; i < 40; ++i)
	{
		ar.ui8(settings.weapTable[i]);
	}
	
	ar
	.ui16(settings.maxBonuses)
	.ui16(settings.blood)
	.ui16(settings.timeToLose)
	.ui16(settings.flagsToWin)
	.ui16(settings.gameMode)
	.b(settings.shadow)
	.b(settings.loadChange)
	.b(settings.namesOnBonuses)
	.b(settings.regenerateLevel)
	.ui16(settings.lives)
	.ui16(settings.loadingTime)
	.b(settings.randomLevel)
	.b(settings.map)
	.b(settings.screenSync)
	.str(settings.levelFile);
	
	// Extensions
	int fileExtensionVersion = Extensions::myVersion;
			
	ar.ui8(fileExtensionVersion);
	
	ar
	.b(settings.extensions)
	.b(settings.recordReplays)
	.b(settings.loadPowerlevelPalette)
	.ui8(settings.scaleFilter)
	.ui16(settings.fullscreenW)
	.ui16(settings.fullscreenH);
	
	gvl::enable_when(ar, fileExtensionVersion >= 2)
		.b(settings.depth32, true);

	ar.check()
	;
}

#endif // LIERO_SETTINGS_HPP
