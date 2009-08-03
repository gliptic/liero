#include "replay.hpp"

#include "game.hpp"
#include "worm.hpp"
#include "viewport.hpp"
#include <gvl/support/type_info.hpp>

inline int32_t uint32_as_int32(uint32_t x)
{
	if(x >= 0x80000000)
		return (int32_t)(x - 0x80000000u) - 0x80000000;
	else
		return (int32_t)x;
}

inline uint32_t int32_as_uint32(int32_t x)
{
	if(x < 0)
		return (uint32_t)(x + 0x80000000) + 0x80000000u;
	else
		return (uint32_t)x;
}

struct WormCreator
{
	Worm* operator()(GameSerializationContext& context)
	{
		return new Worm(*context.game);
	}
};

struct ViewportCreator
{
	Viewport* operator()(GameSerializationContext& context)
	{
		return new Viewport(*context.game);
	}
};

template<typename Context>
struct InArchive
{
	static bool const in = true;
	static bool const out = false;
	
	
	InArchive(gvl::stream_reader& reader, Context& context)
	: reader(reader), context(context)
	{
	}
	
	template<typename T>
	InArchive& i32(T& v)
	{
		v = uint32_as_int32(gvl::read_uint32(reader));
		return *this;
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

	template<typename T>
	InArchive& str(T& v)
	{
		uint32_t len = gvl::read_uint32(reader);
		v.clear();
		for(uint32_t i = 0; i < len; ++i)
		{
			v.push_back((char)reader.get());
		}
		return *this;
	}
	
	template<typename T, typename Creator>
	InArchive& obj(T*& v, Creator creator)
	{
		uint32_t id = gvl::read_uint32(reader);
		if(context.read(v, id, creator))
			archive(*this, *v);
			
		return *this;
	}
	
	template<typename T>
	InArchive& obj(T*& v)
	{
		return obj(v, gvl::new_creator<T>());
	}
	
	template<typename T, typename Creator>
	InArchive& obj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p;
		obj(p, creator);
		v.reset(p);
		return *this;
	}
	
	template<typename T>
	InArchive& obj(gvl::shared_ptr<T>& v)
	{
		return obj(v, gvl::new_creator<T>());
	}
	
	template<typename T, typename Creator>
	InArchive& fobj(T*& v, Creator creator)
	{
		v = creator(context);
		archive(*this, *v);
			
		return *this;
	}
	
	template<typename T>
	InArchive& fobj(T*& v)
	{
		return fobj(v, gvl::new_creator<T>());
	}
	
	template<typename T, typename Creator>
	InArchive& fobj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p;
		fobj(p, creator);
		v.reset(p);
		return *this;
	}
	
	template<typename T>
	InArchive& fobj(gvl::shared_ptr<T>& v)
	{
		return fobj(v, gvl::new_creator<T>());
	}
	
	InArchive& check()
	{
		uint32_t v = gvl::read_uint32(reader);
		if(v != 0x12345678)
			throw std::runtime_error("Expected checkpoint here");
		return *this;
	}
	
	gvl::stream_reader& reader;
	Context& context;
};

template<typename Context>
struct OutArchive
{
	static bool const in = false;
	static bool const out = true;
	
	OutArchive(gvl::stream_writer& writer, Context& context)
	: writer(writer), context(context)
	{
	}
	
	
	
	OutArchive& i32(int32_t v)
	{
		gvl::write_uint32(writer, int32_as_uint32(v));
		return *this;
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

	template<typename T>
	OutArchive& str(T const& v)
	{
		gvl::write_uint32(writer, v.size());
		for(uint32_t i = 0; i < v.size(); ++i)
		{
			writer.put((uint8_t)v[i]);
		}
		return *this;
	}
	
	template<typename T, typename Creator>
	OutArchive& obj(T*& v, Creator creator)
	{
		std::pair<bool, uint32_t> res = context.write(v);
		
		gvl::write_uint32(writer, res.second);
		if(res.first)
			archive(*this, *v);
		
		return *this;
	}
	
	template<typename T>
	OutArchive& obj(T*& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	OutArchive& obj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return obj(p);
	}
	
	template<typename T>
	OutArchive& obj(gvl::shared_ptr<T>& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	OutArchive& fobj(T*& v, Creator creator)
	{
		archive(*this, *v);
		
		return *this;
	}
	
	template<typename T>
	OutArchive& fobj(T*& v)
	{
		return fobj(v, 0);
	}
	
	template<typename T, typename Creator>
	OutArchive& fobj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return fobj(p);
	}
	
	template<typename T>
	OutArchive& fobj(gvl::shared_ptr<T>& v)
	{
		return fobj(v, 0);
	}
	
	OutArchive& check()
	{
		gvl::write_uint32(writer, 0x12345678);
		return *this;
	}
	
	gvl::stream_writer& writer;
	Context& context;
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
	.str(settings.levelFile)

	.check()
	;
}

template<typename Archive>
void archive(Archive ar, Worm::ControlState& cs)
{
	for(int i = 0; i < Worm::MaxControl; ++i)
	{
		ar.b(cs.state[i]);
	}
}

template<typename Archive>
void archive(Archive ar, Worm& worm)
{
	ar
	.i32(worm.x)
	.i32(worm.y)
	.i32(worm.velX)
	.i32(worm.velY)
	.i32(worm.logicRespawnX)
	.i32(worm.logicRespawnY)
	.i32(worm.hotspotX)
	.i32(worm.hotspotY)
	.i32(worm.aimingAngle)
	.i32(worm.aimingSpeed)
	.b(worm.ableToJump)
	.b(worm.ableToDig)
	.b(worm.keyChangePressed)
	.b(worm.movable)
	.b(worm.animate)
	.b(worm.visible)
	.b(worm.ready)
	.b(worm.flag)
	.b(worm.makeSightGreen)
	.i32(worm.health)
	.i32(worm.lives)
	.i32(worm.kills)
	.i32(worm.timer)
	.i32(worm.killedTimer)
	.i32(worm.currentFrame)
	.i32(worm.flags)
	
	.obj(worm.ninjarope.anchor, WormCreator())
	.b(worm.ninjarope.attached)
	.i32(worm.ninjarope.curLen)
	.i32(worm.ninjarope.length)
	.b(worm.ninjarope.out)
	.i32(worm.ninjarope.velX)
	.i32(worm.ninjarope.velY)
	.i32(worm.ninjarope.x)
	.i32(worm.ninjarope.y)
	.i32(worm.currentWeapon)
	.b(worm.fireConeActive)
	.obj(worm.lastKilledBy, WormCreator())
	.i32(worm.fireCone)
	.i32(worm.leaveShellTimer)
	.fobj(worm.settings)
	.i32(worm.index)
	.i32(worm.wormSoundID)
	.i32(worm.reacts[0])
	.i32(worm.reacts[1])
	.i32(worm.reacts[2])
	.i32(worm.reacts[3]);
	
	for(int i = 0; i < 5; ++i)
	{
		ar
		.i32(worm.weapons[i].ammo)
		.b(worm.weapons[i].available)
		.i32(worm.weapons[i].delayLeft)
		.i32(worm.weapons[i].id)
		.i32(worm.weapons[i].loadingLeft);
	}
	
	ar.ui8(worm.direction);
	
	archive(ar, worm.controlStates);
	
	ar.check();
}

template<typename Archive>
void archive(Archive ar, Viewport& vp)
{
	ar
	.i32(vp.x)
	.i32(vp.y)
	.i32(vp.shake)
	.i32(vp.maxX)
	.i32(vp.maxY)
	.i32(vp.centerX)
	.i32(vp.centerY)
	.obj(vp.worm, WormCreator())
	.i32(vp.bannerY)
	.i32(vp.inGameX)
	.i32(vp.rect.x1)
	.i32(vp.rect.y1)
	.i32(vp.rect.x2)
	.i32(vp.rect.y2)
	.ui32(vp.rand.curSeed)
	;
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

void archive(InArchive<GameSerializationContext> ar, Level& level)
{
	unsigned int w = gvl::read_uint16(ar.reader);
	unsigned int h = gvl::read_uint16(ar.reader);
	level.resize(w, h);
	
	for(unsigned int y = 0; y < h; ++y)
	for(unsigned int x = 0; x < w; ++x)
	{
		level.data[y*w + x] = ar.reader.get();
	}
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		level.origpal.entries[i].r = ar.reader.get();
		level.origpal.entries[i].g = ar.reader.get();
		level.origpal.entries[i].b = ar.reader.get();
	}
}

void archive(OutArchive<GameSerializationContext> ar, Level& level)
{
	ar.ui16(level.width);
	ar.ui16(level.height);
	ar.writer.put_bucket(new gvl::bucket(&level.data[0], level.width * level.height));
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		ar.writer.put(level.origpal.entries[i].r);
		ar.writer.put(level.origpal.entries[i].g);
		ar.writer.put(level.origpal.entries[i].b);
	}
}

template<typename Archive>
void archive(Archive ar, WormSettings& ws)
{
	ar
	.ui32(ws.colour)
	.ui32(ws.health)
	.ui16(ws.controller);
	for(int i = 0; i < 5; ++i)
		ar.ui16(ws.weapons[i]);
	for(int i = 0; i < 3; ++i)
		ar.ui16(ws.rgb[i]);
	ar.b(ws.randomName);
	ar.str(ws.name);
}


void archive_worms(InArchive<GameSerializationContext> ar, Game& game)
{
	uint8_t cont;
	while(ar.ui8(cont), cont)
	{
		int wormId = ar.context.nextWormId++;
		
		Worm* worm;
		ar.obj(worm, WormCreator());
		
		GameSerializationContext::WormData& data = ar.context.wormData[worm];
		archive(ar, data.prevControls);
		
		/*
		if(wormId == 0)
			game.addViewport(new Viewport(Rect(0, 0, 158, 158), worm, 0, 504, 350, game));
		else if(wormId == 1)
			game.addViewport(new Viewport(Rect(160, 0, 158+160, 158), worm, 218, 504, 350, game));
			*/
		game.addWorm(worm);
		ar.context.idToWorm[wormId] = worm;
	}
	
	while(ar.ui8(cont), cont)
	{
		Viewport* vp;
		ar.fobj(vp, ViewportCreator());
		
		game.addViewport(vp);
	}
}

void archive_worms(OutArchive<GameSerializationContext> ar, Game& game)
{
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		
		int wormId = ar.context.nextWormId++;
		ar.context.idToWorm[wormId] = &worm;
	}
	
	for(GameSerializationContext::IdToWormMap::iterator i = ar.context.idToWorm.begin(); i != ar.context.idToWorm.end(); ++i)
	{
		ar.writer.put(1);
		
		Worm* worm = i->second;
		
		ar.obj(worm, WormCreator());
			
		GameSerializationContext::WormData& data = ar.context.wormData[worm];
		archive(ar, data.prevControls);
		
	}
	ar.writer.put(0);
	
	for(std::size_t i = 0; i < game.viewports.size(); ++i)
	{
		ar.writer.put(1);
		Viewport* vp = game.viewports[i];
		ar.fobj(vp);
	}
	ar.writer.put(0);
}

template<typename Archive>
void archive(Archive ar, Game& game)
{
	if(ar.in)
		ar.context.game = &game;
	
	ar
	.i32(game.cycles)
	.b(game.gotChanged)
	.obj(game.lastKilled, WormCreator())
	.i32(game.screenFlash)
	.ui32(game.rand.curSeed);
	
	archive_worms(ar, game);
	
	archive(ar, game.level);
}

template<typename T>
void read(gvl::stream_reader& reader, GameSerializationContext& context, T& ws)
{
	archive(InArchive<GameSerializationContext>(reader, context), ws);
}

template<typename T>
void write(gvl::stream_writer& writer, GameSerializationContext& context, T& ws)
{
	archive(OutArchive<GameSerializationContext>(writer, context), ws);
}

std::auto_ptr<Game> Replay::beginPlayback(gvl::shared_ptr<Common> common)
{
	uint8_t version = reader.get();
	gvl::shared_ptr<Settings> settings(new Settings);
	read(reader, context, *settings);
	
	std::auto_ptr<Game> game(new Game(common, settings));
	//context.game = game.get();
	//game->rand.curSeed = gvl::read_uint32(reader);
	//game->cycles = gvl::read_uint32(reader);
	read(reader, context, *game);
	
	uint32_t magic = gvl::read_uint32(reader);
	if(magic != 0x13371337)
		throw "FAIL";
		
	return game;
}

void Replay::beginRecord(Game& game)
{
	context.game = &game;
	writer.put(replayVersion);
	
	write(writer, context, *game.settings);
	//gvl::write_uint32(writer, game.rand.curSeed);
	//gvl::write_uint32(writer, game.cycles);
	write(writer, context, game);
	
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
		for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin(); ;)
		{
			Worm* worm = i->second;
			GameSerializationContext::WormData& data = context.wormData[worm];
			
			for(int c = 0; c < Worm::MaxControl; ++c)
			{
				worm->setControlState(Worm::Control(c), ((state >> c) & 1) != 0);
			}
			
			++i;
			if(i == context.idToWorm.end())
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
	for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin();
		i != context.idToWorm.end();
		++i)
	{
		Worm* worm = i->second;
		GameSerializationContext::WormData& data = context.wormData[worm];
		if(worm->controlStates != data.prevControls)
		{
			writeStates = true;
			break;
		}
	}
	
	if(writeStates)
	{
		for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin();
			i != context.idToWorm.end();
			++i)
		{
			Worm* worm = i->second;
			GameSerializationContext::WormData& data = context.wormData[worm];
			
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