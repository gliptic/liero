#include "replay.hpp"

#include "game.hpp"
#include "worm.hpp"
#include "viewport.hpp"

struct InArchive
{
	static bool const in = true;
	static bool const out = false;
	
	
	InArchive(gvl::stream_reader& reader)
	: reader(reader)
	{
	}
	
	template<typename T>
	InArchive& ui16(T& v)
	{
		v = gvl::read_uint16(reader);
		return *this;
	}
	
	template<typename T>
	InArchive& ui32(T& v)
	{
		v = gvl::read_uint32(reader);
		return *this;
	}
	
	template<typename T>
	InArchive& ui8(T& v)
	{
		v = reader.get();
		return *this;
	}
	
	template<typename T>
	InArchive& b(T& v)
	{
		v = !!reader.get();
		return *this;
	}
	
	gvl::stream_reader& reader;
};

struct OutArchive
{
	static bool const in = false;
	static bool const out = true;
	
	OutArchive(gvl::stream_writer& writer)
	: writer(writer)
	{
	}
	
	
	OutArchive& ui16(uint32_t v)
	{
		gvl::write_uint16(writer, v);
		return *this;
	}
	
	OutArchive& ui32(uint32_t v)
	{
		gvl::write_uint32(writer, v);
		return *this;
	}
	
	OutArchive& ui8(uint32_t v)
	{
		writer.put(v);
		return *this;
	}
	
	OutArchive& b(bool v)
	{
		writer.put(v ? 1 : 0);
		return *this;
	}
	
	gvl::stream_writer& writer;
};

/*
void read(gvl::stream_reader& reader, Settings& settings)
{
	
	WormWeapon
	
	int id;
	int ammo;
	int delayLeft;
	int loadingLeft;
	bool available;	

}
	*/
	
template<typename Archive>
void archive(Archive ar, Settings& settings)
{
	for(int i = 0; i < 40; ++i)
	{
		ar.ui16(settings.weapTable[i]);
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
	.ui16(settings.maxBonuses);
	settings.levelFile = "";
}

/*
void read(gvl::stream_reader& reader, Settings& settings)
{
	for(int i = 0; i < 40; ++i)
	{
		settings.weapTable[i] = gvl::read_uint16(reader);
	}
	settings.maxBonuses = gvl::read_uint16(reader);
	settings.blood = gvl::read_uint16(reader);
	settings.timeToLose = gvl::read_uint16(reader);
	settings.flagsToWin = gvl::read_uint16(reader);
	settings.gameMode = gvl::read_uint16(reader);
	settings.shadow = !!reader.get();
	settings.loadChange = !!reader.get();
	settings.namesOnBonuses = !!reader.get();
	settings.regenerateLevel = !!reader.get();
	settings.lives = gvl::read_uint16(reader);
	settings.loadingTime = gvl::read_uint16(reader);
	settings.randomLevel = !!reader.get();
	settings.levelFile = "";
	settings.map = !!reader.get();
	settings.screenSync = !!reader.get();
}

void write(gvl::stream_writer& writer, Settings& settings)
{
	for(int i = 0; i < 40; ++i)
	{
		gvl::write_uint16(writer, settings.weapTable[i]);
	}
	gvl::write_uint16(writer, settings.maxBonuses);
	gvl::write_uint16(writer, settings.blood);
	gvl::write_uint16(writer, settings.timeToLose);
	gvl::write_uint16(writer, settings.flagsToWin);
	gvl::write_uint16(writer, settings.gameMode);
	writer.put(settings.shadow);
	writer.put(settings.loadChange);
	writer.put(settings.namesOnBonuses);
	writer.put(settings.regenerateLevel);
	gvl::write_uint16(writer, settings.lives);
	gvl::write_uint16(writer, settings.loadingTime);
	writer.put(settings.randomLevel);
	writer.put(settings.map);
	writer.put(settings.screenSync);
}*/

void read(gvl::stream_reader& reader, Level& level)
{
	unsigned int w = gvl::read_uint16(reader);
	unsigned int h = gvl::read_uint16(reader);
	level.resize(w, h);
	
	for(unsigned int y = 0; y < h; ++y)
	for(unsigned int x = 0; x < w; ++x)
	{
		level.data[y*w + x] = reader.get();
	}
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		level.origpal.entries[i].r = reader.get();
		level.origpal.entries[i].g = reader.get();
		level.origpal.entries[i].b = reader.get();
	}
}

void write(gvl::stream_writer& writer, Level& level)
{
	gvl::write_uint16(writer, level.width);
	gvl::write_uint16(writer, level.height);
	writer.put(new gvl::bucket(&level.data[0], level.width * level.height));
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		writer.put(level.origpal.entries[i].r);
		writer.put(level.origpal.entries[i].g);
		writer.put(level.origpal.entries[i].b);
	}
}

template<typename Archive>
void archive(Archive ar, WormSettings& ws)
{
	ar
	.ui32(ws.health)
	.ui16(ws.controller);
	for(int i = 0; i < 5; ++i)
		ar.ui16(ws.weapons[i]);
	for(int i = 0; i < 3; ++i)
		ar.ui16(ws.rgb[i]);
	ar.b(ws.randomName);
}

template<typename Archive>
void archive(Archive ar, Viewport& viewport)
{
	ar
	.ui16(viewport.x)
	.ui16(viewport.y)
	.ui32(viewport.shake)
	.ui16(viewport.maxX)
	.ui16(viewport.maxY)
	.ui16(viewport.centerX)
	.ui16(viewport.centerY)
	
	Worm* worm;
	int bannerY;
	int inGameX; // 0 for first, 218 for second
	Rect rect;
	
}

/*
void write(gvl::stream_writer& writer, WormSettings& ws)
{
	gvl::write_uint32(writer, ws.health);
	gvl::write_uint16(writer, ws.controller);
	for(int i = 0; i < 5; ++i)
		gvl::write_uint16(writer, ws.weapons[i]);
	for(int i = 0; i < 3; ++i)
		gvl::write_uint16(writer, ws.rgb[i]);
	writer.put(ws.randomName ? 1 : 0);
}

void read(gvl::stream_reader& reader, WormSettings& ws)
{
	ws.health = gvl::read_uint32(reader);
	ws.controller = gvl::read_uint16(reader);
	for(int i = 0; i < 5; ++i)
		ws.weapons[i] = gvl::read_uint16(reader);
	for(int i = 0; i < 3; ++i)
		ws.rgb[i] = gvl::read_uint16(reader);
	ws.randomName = !!reader.get();
}*/

template<typename T>
void read(gvl::stream_reader& reader, T& ws)
{
	archive(InArchive(reader), ws);
}

template<typename T>
void write(gvl::stream_writer& writer, T& ws)
{
	archive(OutArchive(writer), ws);
}

std::auto_ptr<Game> Replay::beginPlayback(gvl::shared_ptr<Common> common)
{
	uint8_t version = reader.get();
	gvl::shared_ptr<Settings> settings(new Settings);
	read(reader, *settings);
	
	std::auto_ptr<Game> game(new Game(common, settings));
	game->rand.curSeed = gvl::read_uint32(reader);
	game->cycles = gvl::read_uint32(reader);
	read(reader, game->level);
	
	while(reader.get())
	{
		int wormId = nextWormId++;
		gvl::shared_ptr<WormSettings> ws(new WormSettings);
		//read(reader, *ws);
		Worm* worm = new Worm(ws, wormId, 19+wormId, *game);
		if(wormId == 0)
			game->addViewport(new Viewport(Rect(0, 0, 158, 158), worm, 0, 504, 350, *game));
		else
			game->addViewport(new Viewport(Rect(160, 0, 158+160, 158), worm, 218, 504, 350, *game));
		game->addWorm(worm);
		idToWorm[wormId] = worm;
		
		worm->initWeapons();
		/*
		worm->aimingAngle = gvl::read_uint32(reader);
		worm->direction = reader.get();*/
	}
	
	uint32_t magic = gvl::read_uint32(reader);
	if(magic != 0x13371337)
		throw "FAIL";
		
	return game;
}

void Replay::beginRecord(Game& game)
{
	writer.put(replayVersion);
	
	write(writer, *game.settings);
	gvl::write_uint32(writer, game.rand.curSeed);
	gvl::write_uint32(writer, game.cycles);
	write(writer, game.level);
	
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		
		int wormId = nextWormId++;
		idToWorm[wormId] = &worm;
	}
	
	for(IdToWormMap::iterator i = idToWorm.begin(); i != idToWorm.end(); ++i)
	{
		writer.put(1);
		Worm* worm = i->second;
		WormData& data = wormData[worm];
		// TODO: Serialize worm data
		write(writer, *worm->settings);
		/*
		gvl::write_uint32(writer, worm->aimingAngle);
		writer.put(worm->direction);*/
	}
	writer.put(uint8_t(0));
	
	gvl::write_uint32(writer, 0x13371337);
}

void Replay::playbackFrame(Game& game)
{
	uint8_t first = reader.get();
	if(first >= 0x80)
	{
		// Nothing
	}
	else
	{
		uint8_t state = first;
		for(IdToWormMap::iterator i = idToWorm.begin(); ;)
		{
			Worm* worm = i->second;
			WormData& data = wormData[worm];
			
			for(int c = 0; c < Worm::MaxControl; ++c)
			{
				worm->setControlState(Worm::Control(c), ((state >> c) & 1) != 0);
			}
			
			++i;
			if(i == idToWorm.end())
				break;
			
			state = reader.get();
		}
	}
	
	uint32_t magic = gvl::read_uint32(reader);
	if(magic != 0x13371337)
		throw "FAIL";
}

void Replay::recordFrame(Game& game)
{
	bool writeStates = true; // false;
	for(IdToWormMap::iterator i = idToWorm.begin(); i != idToWorm.end(); ++i)
	{
		Worm* worm = i->second;
		WormData& data = wormData[worm];
		if(worm->controlStates != data.prevControls)
		{
			writeStates = true;
			break;
		}
	}
	
	if(writeStates)
	{
		for(IdToWormMap::iterator i = idToWorm.begin(); i != idToWorm.end(); ++i)
		{
			Worm* worm = i->second;
			WormData& data = wormData[worm];
			
			uint8_t state = 0;
			for(int c = 0; c < Worm::MaxControl; ++c)
			{
				state |= uint8_t(worm->controlStates[c]) << c;
			}
			
			sassert(state < 0x80);
			
			writer.put(state);
			data.prevControls = worm->controlStates;
		}
	}
	else
	{
		writer.put(0x80); // Bit 7 means empty frame
	}
	
	gvl::write_uint32(writer, 0x13371337);
}