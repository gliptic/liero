#include "replay.hpp"

#include "game.hpp"
#include "worm.hpp"
#include "viewport.hpp"
#include <gvl/support/type_info.hpp>
#include <gvl/serialization/archive.hpp>
#include <gvl/io/deflate_filter.hpp>

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
void archive(Archive ar, Worm::ControlState& cs)
{
/*
	for(int i = 0; i < Worm::MaxControl; ++i)
	{
		ar.b(cs.state[i]);
	}*/
	ar.ui8(cs.istate);
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
	.i32(worm.leaveShellTimer);
	ar.fobj(worm.settings)
	.i32(worm.index)
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
	archive(ar, worm.prevControlStates);
	
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
	;
	
	archive(ar, vp.rand);
}

struct mtf
{
	uint8_t order[256];
	
	mtf()
	{
		for(int i = 0; i < 256; ++i)
			order[i] = i;
	}
	
	uint8_t byte_to_rank(uint8_t v)
	{
		for(int i = 0; i < 256; ++i)
		{
			if(order[i] == v)
				return i;
		}
		
		return 0; // Will never reach here
	}
	
	uint8_t rank_to_byte(uint8_t v)
	{
		return order[v];
	}
	
	void promote_rank(uint8_t rank)
	{
		uint8_t byte = order[rank];
		for(uint32_t i = rank; i-- > 0; )
		{
			order[i+1] = order[i];
		}
		order[0] = byte;
	}
};

template<typename Archive>
void archive(Archive ar, Palette& pal)
{
	for(int i = 0; i < 256; ++i)
	{
		ar.ui8(pal.entries[i].r);
		ar.ui8(pal.entries[i].g);
		ar.ui8(pal.entries[i].b);
	}
}

void archive(gvl::in_archive<GameSerializationContext> ar, Level& level)
{
	unsigned int w = gvl::read_uint16(ar.reader);
	unsigned int h = gvl::read_uint16(ar.reader);
	level.resize(w, h);
	
	if(ar.context.replayVersion > 1)
		archive(ar, level.origpal);

#if 1
	for(unsigned int y = 0; y < h; ++y)
	for(unsigned int x = 0; x < w; ++x)
	{
		level.data[y*w + x] = ar.reader.get();
	}
#else
	mtf level_mtf;
	for(unsigned int y = 0; y < h; ++y)
	for(unsigned int x = 0; x < w; ++x)
	{
		uint8_t rank = ar.reader.get();
		uint8_t pix = level_mtf.rank_to_byte(rank);
		level.data[y*w + x] = pix;
		level_mtf.promote_rank(rank);
	}
#endif
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		level.origpal.entries[i].r = ar.reader.get();
		level.origpal.entries[i].g = ar.reader.get();
		level.origpal.entries[i].b = ar.reader.get();
	}
}

template<typename Writer>
void archive(gvl::out_archive<GameSerializationContext, Writer> ar, Level& level)
{
	ar.ui16(level.width);
	ar.ui16(level.height);
	unsigned int w = level.width;
	unsigned int h = level.height;
	
	if(ar.context.replayVersion > 1)
		archive(ar, level.origpal);
	
#if 1
	ar.writer.put(&level.data[0], w * h);
#else
	mtf level_mtf;
	
	for(unsigned int y = 0; y < h; ++y)
	for(unsigned int x = 0; x < w; ++x)
	{
		uint8_t pix = level.data[y*w + x];
		uint8_t rank = level_mtf.byte_to_rank(pix);
		ar.writer.put(rank);
		level_mtf.promote_rank(rank);
	}
#endif
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		ar.writer.put(level.origpal.entries[i].r);
		ar.writer.put(level.origpal.entries[i].g);
		ar.writer.put(level.origpal.entries[i].b);
	}
}

void archive_worms(gvl::in_archive<GameSerializationContext> ar, Game& game)
{
	uint8_t cont;
	while(ar.ui8(cont), cont)
	{
		int wormId = ar.context.nextWormId++;
		
		Worm* worm;
		ar.obj(worm, WormCreator());
		
		//printf("Worm ID %d: %s\n", wormId, worm->settings->name.c_str());
		
		//GameSerializationContext::WormData& data = ar.context.wormData[worm];

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

template<typename Writer>
void archive_worms(gvl::out_archive<GameSerializationContext, Writer> ar, Game& game)
{
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		
		int wormId = ar.context.nextWormId++;
		
		//printf("Worm from game, ID %d: %s\n", wormId, worm.settings->name.c_str());
		ar.context.idToWorm[wormId] = &worm;
	}
	
	for(GameSerializationContext::IdToWormMap::iterator i = ar.context.idToWorm.begin(); i != ar.context.idToWorm.end(); ++i)
	{
		ar.writer.put(1);
		
		Worm* worm = i->second;
		GameSerializationContext::WormData& data = ar.context.wormData[worm];
		
		ar.obj(worm, WormCreator());
		
		//printf("Worm ID %d: %s\n", i->first, worm->settings->name.c_str());
		
		data.settingsExpired = false; // We just serialized them, so they have to be up to date
		
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
	ar.context.game = &game;
	
	ar
	.fobj(game.settings)
	.i32(game.cycles)
	.b(game.gotChanged)
	.obj(game.lastKilled, WormCreator())
	.i32(game.screenFlash);
	archive(ar, game.rand);
	
	archive_worms(ar, game);
	
	archive(ar, game.level);
}

template<typename T>
void read(gvl::octet_stream_reader& reader, GameSerializationContext& context, T& x)
{
	archive(gvl::in_archive<GameSerializationContext>(reader, context), x);
}

template<typename T>
void write(gvl::octet_stream_writer& writer, GameSerializationContext& context, T& x)
{
	archive(gvl::out_archive<GameSerializationContext>(writer, context), x);
}

template<typename T>
gvl::gash::value_type hash(T& x)
{
	GameSerializationContext context;
	gvl::hash_accumulator<gvl::gash> ha;
	
	archive(gvl::out_archive<GameSerializationContext, gvl::hash_accumulator<gvl::gash> >(ha, context), x);
	
	ha.flush();
	return ha.final();
}

template<typename Archive>
void archive(Archive ar, gvl::gash::value_type& x)
{
	for(int i = 0; i < gvl::gash::value_type::size; ++i)
	{
		uint32_t h = uint32_t(x.value[i] >> 32);
		uint32_t l = uint32_t(x.value[i] & 0xffffffff);
		ar.ui32(l);
		ar.ui32(h);
		x.value[i] = (uint64_t(h) << 32) | l;
	}
}

ReplayWriter::ReplayWriter(gvl::stream_ptr str_init)
: settingsExpired(true)
{
#if 1
	str.reset(new gvl::deflate_filter(true));
	str->attach_sink(str_init);
#else
	str = str_init;
#endif
	writer.attach(str);
}

ReplayWriter::~ReplayWriter()
{
	endRecord();
}

ReplayReader::ReplayReader(gvl::stream_ptr str_init)
{
#if 1
	str.reset(new gvl::deflate_filter(false));
	str->attach_source(str_init);
#else
	str = str_init;
#endif
	reader.attach(str);
}

//#define DEBUG_REPLAYS

uint32_t const replayMagic = ('L' << 24) | ('R' << 16) | ('P' << 8) | 'F';

std::auto_ptr<Game> ReplayReader::beginPlayback(gvl::shared_ptr<Common> common)
{
	uint32_t readMagic = gvl::read_uint32(reader);
	if(readMagic != replayMagic)
		throw gvl::archive_check_error("File does not appear to be a replay");
	context.replayVersion = reader.get();
	if(context.replayVersion > myGameVersion)
		throw gvl::archive_check_error("Replay version is too recent");
	
	gvl::shared_ptr<Settings> settings(new Settings);

	std::auto_ptr<Game> game(new Game(common, settings));

	read(reader, context, *game);
#ifdef DEBUG_REPLAYS
	gvl::gash::value_type actualH = hash(*game);		
	gvl::gash::value_type expectedH;
	read(reader, context, expectedH);
	
	if(actualH != expectedH)
		std::cout << "Differing hashes" << std::endl;
#endif
	return game;
}

void ReplayWriter::beginRecord(Game& game)
{
	gvl::write_uint32(writer, replayMagic);
	writer.put(context.replayVersion);
	
	write(writer, context, game);
	settingsExpired = false; // We just serialized them, so they have to be up to date
	
#ifdef DEBUG_REPLAYS
	gvl::gash::value_type h = hash(game);
	write(writer, context, h);
#endif
}

void ReplayWriter::endRecord()
{
	writer.put(0x83);
}

uint32_t fastGameChecksum(Game& game)
{
	// game.rand is like a golden thread
	uint32_t checksum = game.rand.x;
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		checksum ^= worm.x;
		checksum += worm.velX;
		checksum ^= worm.y;
		checksum += worm.velY;
	}
	
	return checksum;
}

bool ReplayReader::playbackFrame()
{
	Game& game = *context.game;
	
	bool settingsChanged = false;
	
	while(true)
	{
		uint8_t first = reader.get();
		
		if(first == 0x80)
			break;
		else if(first == 0x81)
		{
			read(reader, context, *game.settings);
			settingsChanged = true;
		}
		else if(first == 0x82)
		{
			uint32_t wormId = gvl::read_uint32(reader);
			GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.find(wormId);
			if(i != context.idToWorm.end())
			{
				read(reader, context, *i->second->settings);
				settingsChanged = true;
			}
		}
		else if(first == 0x83)
		{
			// End of replay
			return false;
		}
		else if(first < 0x80)
		{
			uint8_t state = first;
			for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin(); ;)
			{
				Worm* worm = i->second;
				//GameSerializationContext::WormData& data = context.wormData[worm];
				
				worm->controlStates.unpack(state ^ worm->prevControlStates.pack());
				
				++i;
				if(i == context.idToWorm.end())
					break;
				
				state = reader.get();
			}
			
			break; // Read frame
		}
		else
			throw gvl::archive_check_error("Unexpected header byte");
	}
	
	if(settingsChanged)
	{
		game.updateSettings();
	}
	
	if((game.cycles % (70 * 15)) == 0)
	{
		uint32_t expected = gvl::read_uint32(reader);
		uint32_t actual = fastGameChecksum(game);
		if(actual != expected)
			throw gvl::archive_check_error("Replay has desynced");
	}
	
#ifdef DEBUG_REPLAYS
	uint32_t expected = gvl::read_uint32(reader);
	uint32_t expected2 = gvl::read_uint32(reader);
	gvl::gash::value_type actual = hash(game);
	if(expected != (uint32_t)actual.value[0])
	{
		std::cout << "Expected: " << expected << ", was: " << (uint32_t)actual.value[0] << std::endl;
		std::cout << "Frame: " << game.cycles << std::endl;
		throw gvl::archive_check_error("Desynced state");
	}
	if(expected2 != game.cycles)
	{
		throw gvl::archive_check_error("Descyned stream");
	}
#endif
	
	return true;
}


void ReplayWriter::recordFrame()
{
	Game& game = *context.game;
	
	if(settingsExpired)
	{
		writer.put(0x81);
		write(writer, context, *context.game->settings);
		settingsExpired = false;
	}
	
	bool writeStates = false;
	
	if(context.idToWorm.size() <= 3) // TODO: What limit do we want here? None?
		writeStates = true;
	else
	{
		for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin();
			i != context.idToWorm.end();
			++i)
		{
			Worm* worm = i->second;
			GameSerializationContext::WormData& data = context.wormData[worm];
			if(worm->controlStates != worm->prevControlStates)
			{
				writeStates = true;
			}
			
			if(data.settingsExpired)
			{
				writer.put(0x82);
				gvl::write_uint32(writer, i->first);
				write(writer, context, *worm->settings);
				data.settingsExpired = false;
			}
		}
	}
	
	if(writeStates)
	{
		for(GameSerializationContext::IdToWormMap::iterator i = context.idToWorm.begin();
			i != context.idToWorm.end();
			++i)
		{
			Worm* worm = i->second;
			//GameSerializationContext::WormData& data = context.wormData[worm];
			
			uint8_t state = worm->controlStates.pack() ^ worm->prevControlStates.pack();
			
			sassert(state < 0x80);
			
			writer.put(state);
		}
	}
	else
	{
		writer.put(0x80); // Bit 7 means empty frame
	}
	
	if((game.cycles % (70 * 15)) == 0)
	{
		uint32_t checksum = fastGameChecksum(game);
		gvl::write_uint32(writer, checksum);
	}
	
#ifdef DEBUG_REPLAYS
	gvl::gash::value_type actual = hash(game);
	gvl::write_uint32(writer, (uint32_t)actual.value[0]);
	gvl::write_uint32(writer, game.cycles);
#endif
}

void ReplayWriter::unfocus()
{
	for(GameSerializationContext::WormDataMap::iterator i = context.wormData.begin();
		i != context.wormData.end();
		++i)
	{
		i->second.lastSettingsHash = i->first->settings->updateHash();
	}
	
	lastSettingsHash = context.game->settings->updateHash();
}

void ReplayWriter::focus()
{
	// TODO: Check hash of settings for game and all worms,
	// mark differing ones for reserialization.
	
	for(GameSerializationContext::WormDataMap::iterator i = context.wormData.begin();
		i != context.wormData.end();
		++i)
	{
		if(i->second.lastSettingsHash != i->first->settings->updateHash())
		{
			i->second.settingsExpired = true;
		}
	}
	
	if(lastSettingsHash != context.game->settings->updateHash())
		settingsExpired = true;
}
