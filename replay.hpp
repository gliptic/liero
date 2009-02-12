#ifndef UUID_4CF92C398C724F883A02E8A68FE1584F
#define UUID_4CF92C398C724F883A02E8A68FE1584F

#include <gvl/io/stream.hpp>
#include <cstring>
#include <map>
#include <memory>
#include "worm.hpp"
#include "common.hpp"

struct Game;

struct Replay
{
	static int const replayVersion = 1;
	
	struct WormData
	{
		Worm::ControlState prevControls;
	};
	
	Replay(gvl::stream_ptr str_init)
	: str(str_init)
	, writer(str_init)
	, reader(str_init)
	, nextWormId(0)
	{
	}
	
	typedef std::map<int, Worm*> IdToWormMap;
	typedef std::map<Worm*, WormData> WormDataMap;
	
	
	std::auto_ptr<Game> beginPlayback(gvl::shared_ptr<Common> common);
	void beginRecord(Game& game);
	
	void playbackFrame(Game& game);
	
	/*
	void setControlState(Worm* worm, Worm::Control control, bool state)
	{
		WormData& data = wormData[worm];
		data.curControls.state[control] = state;
	}*/
	
	void recordFrame(Game& game);
	
	
	
	
	gvl::stream_ptr str;
	gvl::stream_writer writer;
	gvl::stream_reader reader;
	WormDataMap wormData;
	IdToWormMap idToWorm;
	int nextWormId;
};

#endif // UUID_4CF92C398C724F883A02E8A68FE1584F
