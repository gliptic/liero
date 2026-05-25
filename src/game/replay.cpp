#include "replay.hpp"

#include "game.hpp"
#include "worm.hpp"
#include "viewport.hpp"
#include "io/coding.hpp"

#include <serialization/cereal_types.hpp>
#include <cereal/archives/portable_binary.hpp>

#include <cassert>
#include <sstream>

//#define DEBUG_REPLAYS 1

// Helper: serialize an object to a binary blob via cereal and write
// [uint32 length][blob] into the replay stream.
template <typename T>
static void cerealWrite(io::Writer& writer, T& obj) {
	std::ostringstream ss(std::ios::binary);
	{
		cereal::PortableBinaryOutputArchive ar(ss);
		ar(obj);
	}
	std::string buf = ss.str();
	io::write_uint32(writer, static_cast<uint32_t>(buf.size()));
	writer.put(reinterpret_cast<uint8_t const*>(buf.data()), buf.size());
}

// Helper: read [uint32 length][blob] from the replay stream and
// deserialize into obj via cereal.
template <typename T>
static void cerealRead(io::MemReader& reader, T& obj) {
	uint32_t len = io::read_uint32(reader);
	std::string buf(len, '\0');
	for (uint32_t i = 0; i < len; ++i)
		buf[i] = static_cast<char>(reader.get());
	std::istringstream ss(buf, std::ios::binary);
	{
		cereal::PortableBinaryInputArchive ar(ss);
		ar(obj);
	}
}

ReplayWriter::ReplayWriter(std::unique_ptr<io::Writer> sink)
: writer(std::move(sink))
, settingsExpired(true)
{
}

ReplayWriter::~ReplayWriter()
{
	endRecord();
}

ReplayReader::ReplayReader(std::unique_ptr<io::Reader> source)
{
	io::InflateReader inflater(std::move(source));
	uint8_t buf[4096];
	for (;;) {
		std::size_t got = inflater.try_get(buf, sizeof(buf));
		if (got == 0)
			break;
		data.insert(data.end(), buf, buf + got);
	}
	reader.reset(data.data(), data.size());
}

uint32_t const replayMagic = ('L' << 24) | ('R' << 16) | ('P' << 8) | 'F';

std::unique_ptr<Game> ReplayReader::beginPlayback(std::shared_ptr<Common> common, std::shared_ptr<SoundPlayer> soundPlayer)
{
	uint32_t readMagic = io::read_uint32(reader);
	if(readMagic != replayMagic)
		throw io::ArchiveCheckError("File does not appear to be a replay");
	replayVersion = reader.get();
	if(replayVersion > myReplayVersion)
		throw io::ArchiveCheckError("Replay version is too recent");

	std::shared_ptr<Settings> settings(new Settings);
	std::unique_ptr<Game> game(new Game(common, settings, soundPlayer));

	cerealRead(reader, *game);

	return game;
}

void ReplayWriter::beginRecord(Game& game)
{
	io::write_uint32(writer, replayMagic);
	writer.put(myReplayVersion);

	cerealWrite(writer, game);
	settingsExpired = false;

	// Track worm settings for change detection
	for (auto const& worm_sp : game.worms) {
		WormData& data = wormData[worm_sp.get()];
		data.settingsExpired = false;
		data.lastSettingsHash = worm_sp->settings->updateHash();
	}
	lastSettingsHash = game.settings->updateHash();
	this->game = &game;
}

void ReplayWriter::endRecord()
{
	writer.put(0x83);
}

uint32_t fastGameChecksum(Game& game)
{
	uint32_t checksum = game.rand.last;
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		checksum ^= worm.pos.x;
		checksum += worm.vel.x;
		checksum ^= worm.pos.y;
		checksum += worm.vel.y;
	}

	return checksum;
}

bool ReplayReader::playbackFrame(Renderer& renderer)
{
	Game& game = *this->game;

	bool settingsChanged = false;

	while(true)
	{
		uint8_t first = reader.get();

		if(first == 0x80)
			break;
		else if(first == 0x81)
		{
			cerealRead(reader, *game.settings);
			settingsChanged = true;
		}
		else if(first == 0x82)
		{
			uint32_t wormId = io::read_uint32(reader);
			Worm* w = game.wormByIdx(wormId);
			if (w)
			{
				cerealRead(reader, *w->settings);
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
			bool hasState = true;

			for (auto const& worm : game.worms)
			{
				if (!hasState)
					state = reader.get();
				else
					hasState = false;

				worm->controlStates.unpack(state ^ worm->prevControlStates.pack());
			}

			break; // Read frame
		}
		else
			throw io::ArchiveCheckError("Unexpected header byte");
	}

	if(settingsChanged)
	{
		game.updateSettings(renderer);
	}

	if((game.cycles % (70 * 15)) == 0)
	{
		uint32_t expected = io::read_uint32(reader);
		uint32_t actual = fastGameChecksum(game);
		if(actual != expected)
			throw io::ArchiveCheckError("Replay has desynced");
	}

	return true;
}


void ReplayWriter::recordFrame()
{
	Game& game = *this->game;

	if(settingsExpired)
	{
		writer.put(0x81);
		cerealWrite(writer, *game.settings);
		settingsExpired = false;
	}

	bool writeStates = false;

	if(game.worms.size() <= 3) // TODO: What limit do we want here? None?
		writeStates = true;
	else
	{
		for (auto const& worm : game.worms)
		{
			WormData& data = wormData[worm.get()];
			if(worm->controlStates != worm->prevControlStates)
			{
				writeStates = true;
			}

			if(data.settingsExpired)
			{
				writer.put(0x82);
				io::write_uint32(writer, worm->index);
				cerealWrite(writer, *worm->settings);
				data.settingsExpired = false;
			}
		}
	}

	if(writeStates)
	{
		for (auto const& worm : game.worms)
		{
			uint8_t state = worm->controlStates.pack() ^ worm->prevControlStates.pack();

			assert(state < 0x80);

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
		io::write_uint32(writer, checksum);
	}
}

void ReplayWriter::unfocus()
{
	for (auto& [worm, data] : wormData)
		data.lastSettingsHash = worm->settings->updateHash();
	lastSettingsHash = game->settings->updateHash();
}

void ReplayWriter::focus()
{
	for (auto& [worm, data] : wormData)
	{
		if(data.lastSettingsHash != worm->settings->updateHash())
			data.settingsExpired = true;
	}

	if(lastSettingsHash != game->settings->updateHash())
		settingsExpired = true;
}
