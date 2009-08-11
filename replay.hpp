#ifndef UUID_4CF92C398C724F883A02E8A68FE1584F
#define UUID_4CF92C398C724F883A02E8A68FE1584F

#include <gvl/io/stream.hpp>
#include <gvl/serialization/context.hpp>
#include <gvl/crypt/gash.hpp>
#include <cstring>
#include <map>
#include <memory>
#include "worm.hpp"
#include "common.hpp"

struct Game;

struct GameSerializationContext : gvl::serialization_context<GameSerializationContext>
{
	static int const myReplayVersion = 1;
	
	GameSerializationContext()
	: game(0)
	, nextWormId(0)
	, replayVersion(myReplayVersion)
	{
	}
	
	struct WormData
	{
		WormData()
		: settingsExpired(true)
		{
		}
		
		gvl::gash::value_type lastSettingsHash;
		bool settingsExpired;
	};
	
	typedef std::map<int, Worm*> IdToWormMap;
	typedef std::map<Worm*, WormData> WormDataMap;
	
	
	Game* game;
	WormDataMap wormData;
	IdToWormMap idToWorm;
	int nextWormId;
	int replayVersion;
};

struct Replay
{
	
	
	Replay()
	{
	}
	/*
	virtual void unfocus() = 0;
	virtual void focus() = 0;
	*/
	
	GameSerializationContext context;
	
};

struct ReplayWriter : Replay
{
	ReplayWriter(gvl::stream_ptr str_init);
	~ReplayWriter();
	
	void unfocus();
	void focus();
	
	gvl::filter_ptr str;
	gvl::stream_writer writer;
	gvl::gash::value_type lastSettingsHash;
	bool settingsExpired;
	
	void beginRecord(Game& game);
	void recordFrame();
private:
	void endRecord();
};

struct ReplayReader : Replay
{
	ReplayReader(gvl::stream_ptr str_init);
	
	void unfocus()
	{
		// Nothing
	}
	
	void focus()
	{
		// Nothing
	}
	
	std::auto_ptr<Game> beginPlayback(gvl::shared_ptr<Common> common);
	bool playbackFrame();
	
	gvl::filter_ptr str;
	gvl::stream_reader reader;
};

#endif // UUID_4CF92C398C724F883A02E8A68FE1584F
