#include "replay.hpp"

#include "game.hpp"
#include "io/coding.hpp"
#include "viewport.hpp"
#include "worm.hpp"

#include <cereal/archives/portable_binary.hpp>
#include <serialization/cereal_types.hpp>

#include <cassert>
#include <sstream>

// #define DEBUG_REPLAYS 1

// Replay stream framing. Bytes < 0x80 are per-frame worm-state deltas
// (a single byte per worm, sequencing implicit in the frame loop).
// Bytes with bit 7 set are tagged records:
namespace {
constexpr uint8_t kReplayTagEmptyFrame = 0x80;    // frame with no input change
constexpr uint8_t kReplayTagSettings = 0x81;      // cereal'd Settings follows
constexpr uint8_t kReplayTagWormSettings = 0x82;  // worm index + cereal'd WormSettings
constexpr uint8_t kReplayTagEnd = 0x83;           // end of stream
}  // namespace

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
  for (uint32_t i = 0; i < len; ++i) buf[i] = static_cast<char>(reader.get());
  std::istringstream ss(buf, std::ios::binary);
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(obj);
  }
}

ReplayWriter::ReplayWriter(std::unique_ptr<io::Writer> sink)
    : writer(std::move(sink)), settingsExpired(true) {}

ReplayWriter::~ReplayWriter() { endRecord(); }

ReplayReader::ReplayReader(std::unique_ptr<io::Reader> source) {
  io::InflateReader inflater(std::move(source));
  uint8_t buf[4096];
  for (;;) {
    std::size_t got = inflater.try_get(buf, sizeof(buf));
    if (got == 0) break;
    data.insert(data.end(), buf, buf + got);
  }
  reader.reset(data.data(), data.size());
}

uint32_t const replayMagic = ('L' << 24) | ('R' << 16) | ('P' << 8) | 'F';

std::unique_ptr<Game> ReplayReader::beginPlayback(std::shared_ptr<Common> common,
                                                  std::shared_ptr<SoundPlayer> soundPlayer) {
  uint32_t readMagic = io::read_uint32(reader);
  if (readMagic != replayMagic) throw io::ArchiveCheckError("File does not appear to be a replay");
  replayVersion = reader.get();
  if (replayVersion > myReplayVersion) throw io::ArchiveCheckError("Replay version is too recent");

  std::shared_ptr<Settings> settings(new Settings);
  std::unique_ptr<Game> game(new Game(common, settings, soundPlayer));

  cerealRead(reader, *game);

  return game;
}

void ReplayWriter::beginRecord(Game& game) {
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

void ReplayWriter::endRecord() { writer.put(kReplayTagEnd); }

namespace {
inline void mix32(uint32_t& h, uint32_t v) { h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2); }
inline void mixBytes(uint32_t& h, void const* p, std::size_t n) {
  auto* b = static_cast<uint8_t const*>(p);
  for (std::size_t i = 0; i < n; ++i) mix32(h, b[i]);
}
}  // namespace

uint32_t wideRollbackChecksum(Game& game) {
  // Folds in projectile pools, level damage, and per-worm sim state the
  // fast checksum drops. Order matters for reproducibility — both peers
  // walk the same fields in the same sequence.
  uint32_t h = game.rand.last;
  mix32(h, static_cast<uint32_t>(game.cycles));

  for (auto const& w_sp : game.worms) {
    Worm const& w = *w_sp;
    mix32(h, static_cast<uint32_t>(w.pos.x));
    mix32(h, static_cast<uint32_t>(w.pos.y));
    mix32(h, static_cast<uint32_t>(w.vel.x));
    mix32(h, static_cast<uint32_t>(w.vel.y));
    mix32(h, static_cast<uint32_t>(w.aimingAngle));
    mix32(h, static_cast<uint32_t>(w.aimingSpeed));
    mix32(h, static_cast<uint32_t>(w.health));
    mix32(h, static_cast<uint32_t>(w.lives));
    mix32(h, static_cast<uint32_t>(w.kills));
    mix32(h, static_cast<uint32_t>(w.timer));
    mix32(h, static_cast<uint32_t>(w.killedTimer));
    mix32(h, static_cast<uint32_t>(w.currentFrame));
    mix32(h, static_cast<uint32_t>(w.currentWeapon));
    mix32(h, static_cast<uint32_t>(w.direction));
    mix32(h, static_cast<uint32_t>(w.flags));
    mix32(h, w.visible ? 1u : 0u);
    mix32(h, w.ready ? 1u : 0u);
    mix32(h, w.ableToJump ? 1u : 0u);
    mix32(h, w.ableToDig ? 1u : 0u);
    mix32(h, static_cast<uint32_t>(w.controlStates.istate));
    mix32(h, static_cast<uint32_t>(w.prevControlStates.istate));
    for (int i = 0; i < NUM_WEAPONS; ++i) {
      WormWeapon const& ww = w.weapons[i];
      mix32(h, static_cast<uint32_t>(ww.ammo));
      mix32(h, static_cast<uint32_t>(ww.delayLeft));
      mix32(h, static_cast<uint32_t>(ww.loadingLeft));
    }
  }

  mix32(h, static_cast<uint32_t>(game.wobjects.count));
  for (std::size_t i = 0; i < game.wobjects.count; ++i) {
    WObject const& o = game.wobjects.arr[i];
    mix32(h, static_cast<uint32_t>(o.pos.x));
    mix32(h, static_cast<uint32_t>(o.pos.y));
    mix32(h, static_cast<uint32_t>(o.vel.x));
    mix32(h, static_cast<uint32_t>(o.vel.y));
    mix32(h, static_cast<uint32_t>(o.curFrame));
    mix32(h, static_cast<uint32_t>(o.timeLeft));
    mix32(h, static_cast<uint32_t>(o.ownerIdx));
  }
  mix32(h, static_cast<uint32_t>(game.sobjects.count));
  for (std::size_t i = 0; i < game.sobjects.count; ++i) {
    SObject const& o = game.sobjects.arr[i];
    mix32(h, static_cast<uint32_t>(o.x));
    mix32(h, static_cast<uint32_t>(o.y));
    mix32(h, static_cast<uint32_t>(o.curFrame));
    mix32(h, static_cast<uint32_t>(o.id));
  }
  mix32(h, static_cast<uint32_t>(game.nobjects.count));
  for (std::size_t i = 0; i < game.nobjects.count; ++i) {
    NObject const& o = game.nobjects.arr[i];
    mix32(h, static_cast<uint32_t>(o.pos.x));
    mix32(h, static_cast<uint32_t>(o.pos.y));
    mix32(h, static_cast<uint32_t>(o.vel.x));
    mix32(h, static_cast<uint32_t>(o.vel.y));
    mix32(h, static_cast<uint32_t>(o.curFrame));
  }
  mix32(h, static_cast<uint32_t>(game.bonuses.count));
  for (std::size_t i = 0; i < game.bonuses.count; ++i) {
    Bonus const& b = game.bonuses.arr[i];
    mix32(h, static_cast<uint32_t>(b.x));
    mix32(h, static_cast<uint32_t>(b.y));
    mix32(h, static_cast<uint32_t>(b.frame));
    mix32(h, static_cast<uint32_t>(b.timer));
  }

  std::size_t const cells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
  if (cells > 0) {
    mixBytes(h, game.level.data.data(), cells);
  }

  return h;
}

bool ReplayReader::playbackFrame(Renderer& renderer) {
  Game& game = *this->game;

  bool settingsChanged = false;

  while (true) {
    uint8_t first = reader.get();

    if (first == kReplayTagEmptyFrame)
      break;
    else if (first == kReplayTagSettings) {
      cerealRead(reader, *game.settings);
      settingsChanged = true;
    } else if (first == kReplayTagWormSettings) {
      uint32_t wormId = io::read_uint32(reader);
      Worm* w = game.wormByIdx(wormId);
      if (w) {
        cerealRead(reader, *w->settings);
        settingsChanged = true;
      }
    } else if (first == kReplayTagEnd) {
      // End of replay
      return false;
    } else if (first < kReplayTagEmptyFrame) {
      uint8_t state = first;
      bool hasState = true;

      for (auto const& worm : game.worms) {
        if (!hasState)
          state = reader.get();
        else
          hasState = false;

        worm->controlStates.unpack(state ^ worm->prevControlStates.pack());
      }

      break;  // Read frame
    } else
      throw io::ArchiveCheckError("Unexpected header byte");
  }

  if (settingsChanged) {
    game.updateSettings(renderer);
  }

  if ((game.cycles % (70 * 15)) == 0) {
    uint32_t expected = io::read_uint32(reader);
    uint32_t actual = wideRollbackChecksum(game);
    if (actual != expected) throw io::ArchiveCheckError("Replay has desynced");
  }

  return true;
}

void ReplayWriter::recordFrame() {
  Game& game = *this->game;

  if (settingsExpired) {
    writer.put(kReplayTagSettings);
    cerealWrite(writer, *game.settings);
    settingsExpired = false;
  }

  bool writeStates = false;

  if (game.worms.size() <= 3)  // TODO: What limit do we want here? None?
    writeStates = true;
  else {
    for (auto const& worm : game.worms) {
      WormData& data = wormData[worm.get()];
      if (worm->controlStates != worm->prevControlStates) {
        writeStates = true;
      }

      if (data.settingsExpired) {
        writer.put(kReplayTagWormSettings);
        io::write_uint32(writer, worm->index);
        cerealWrite(writer, *worm->settings);
        data.settingsExpired = false;
      }
    }
  }

  if (writeStates) {
    for (auto const& worm : game.worms) {
      uint8_t state = worm->controlStates.pack() ^ worm->prevControlStates.pack();

      assert(state < kReplayTagEmptyFrame);

      writer.put(state);
    }
  } else {
    writer.put(kReplayTagEmptyFrame);
  }

  if ((game.cycles % (70 * 15)) == 0) {
    uint32_t checksum = wideRollbackChecksum(game);
    io::write_uint32(writer, checksum);
  }
}

void ReplayWriter::unfocus() {
  for (auto& [worm, data] : wormData) data.lastSettingsHash = worm->settings->updateHash();
  lastSettingsHash = game->settings->updateHash();
}

void ReplayWriter::focus() {
  for (auto& [worm, data] : wormData) {
    if (data.lastSettingsHash != worm->settings->updateHash()) data.settingsExpired = true;
  }

  if (lastSettingsHash != game->settings->updateHash()) settingsExpired = true;
}
