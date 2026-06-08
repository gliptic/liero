#include "replay.hpp"

#include "game.hpp"
#include "io/coding.hpp"
#include "viewport.hpp"
#include "worm.hpp"

#include <cereal/archives/portable_binary.hpp>
#include <memory>
#include <serialization/cereal_types.hpp>

#include <cassert>
#include <sstream>
#include <utility>

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
static void CerealWrite(io::Writer& writer, T& obj) {
  std::ostringstream ss(std::ios::binary);
  {
    // Cereal archives mutate state on operator(); they cannot be const.
    cereal::PortableBinaryOutputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(obj);
  }
  std::string buf = ss.str();
  io::WriteUint32(writer, static_cast<uint32_t>(buf.size()));
  writer.Put(reinterpret_cast<uint8_t const*>(buf.data()), buf.size());
}

// Helper: read [uint32 length][blob] from the replay stream and
// deserialize into obj via cereal.
template <typename T>
static void CerealRead(io::MemReader& reader, T& obj) {
  uint32_t const kLen = io::ReadUint32(reader);
  std::string buf(kLen, '\0');
  for (uint32_t i = 0; i < kLen; ++i) buf[i] = static_cast<char>(reader.Get());
  std::istringstream ss(buf, std::ios::binary);
  {
    cereal::PortableBinaryInputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(obj);
  }
}

ReplayWriter::ReplayWriter(std::unique_ptr<io::Writer> sink) : writer(std::move(sink)) {}

ReplayWriter::~ReplayWriter() {
  try {
    EndRecord();
  } catch (...) {  // NOLINT(bugprone-empty-catch) — destructor: writes during destruction are
                   // best-effort.
  }
}

ReplayReader::ReplayReader(std::unique_ptr<io::Reader> source) {
  io::InflateReader inflater(std::move(source));
  uint8_t buf[4096];
  for (;;) {
    std::size_t const kGot = inflater.TryGet(buf, sizeof(buf));
    if (kGot == 0) break;
    data.insert(data.end(), buf, buf + kGot);
  }
  reader.Reset(data.data(), data.size());
}

uint32_t const kReplayMagic = ('L' << 24) | ('R' << 16) | ('P' << 8) | 'F';

std::unique_ptr<Game> ReplayReader::BeginPlayback(
    const std::shared_ptr<Common>& common, const std::shared_ptr<SoundPlayer>& sound_player) {
  uint32_t const kReadMagic = io::ReadUint32(reader);
  if (kReadMagic != kReplayMagic)
    throw io::ArchiveCheckError("File does not appear to be a replay");
  replay_version = reader.Get();
  if (replay_version > kMyReplayVersion)
    throw io::ArchiveCheckError("Replay version is too recent");

  std::shared_ptr<Settings> const kSettings = std::make_shared<Settings>();
  std::unique_ptr<Game> game(new Game(common, kSettings, sound_player));

  CerealRead(reader, *game);

  return game;
}

void ReplayWriter::BeginRecord(Game& game) {
  io::WriteUint32(writer, kReplayMagic);
  writer.Put(kMyReplayVersion);

  CerealWrite(writer, game);
  settings_expired = false;

  // Track worm settings for change detection
  for (auto const& worm_sp : game.worms) {
    WormData& data = worm_data[worm_sp.get()];
    data.settings_expired = false;
    data.last_settings_hash = worm_sp->settings->UpdateHash();
  }
  last_settings_hash = game.settings->UpdateHash();
  this->game = &game;
}

void ReplayWriter::EndRecord() { writer.Put(kReplayTagEnd); }

namespace {
inline void Mix32(uint32_t& h, uint32_t v) { h ^= v + 0x9e3779b9U + (h << 6) + (h >> 2); }
inline void MixBytes(uint32_t& h, void const* p, std::size_t n) {
  const auto* b = static_cast<uint8_t const*>(p);
  for (std::size_t i = 0; i < n; ++i) Mix32(h, b[i]);
}
}  // namespace

uint32_t WideRollbackChecksum(Game& game) {
  // Folds in projectile pools, level damage, and per-worm sim state the
  // fast checksum drops. Order matters for reproducibility — both peers
  // walk the same fields in the same sequence.
  uint32_t h = game.rand.last;
  Mix32(h, static_cast<uint32_t>(game.cycles));

  for (auto const& w_sp : game.worms) {
    Worm const& w = *w_sp;
    Mix32(h, static_cast<uint32_t>(w.pos.x));
    Mix32(h, static_cast<uint32_t>(w.pos.y));
    Mix32(h, static_cast<uint32_t>(w.vel.x));
    Mix32(h, static_cast<uint32_t>(w.vel.y));
    Mix32(h, static_cast<uint32_t>(w.aiming_angle));
    Mix32(h, static_cast<uint32_t>(w.aiming_speed));
    Mix32(h, static_cast<uint32_t>(w.health));
    Mix32(h, static_cast<uint32_t>(w.lives));
    Mix32(h, static_cast<uint32_t>(w.kills));
    Mix32(h, static_cast<uint32_t>(w.timer));
    Mix32(h, static_cast<uint32_t>(w.killed_timer));
    Mix32(h, static_cast<uint32_t>(w.current_frame));
    Mix32(h, static_cast<uint32_t>(w.current_weapon));
    Mix32(h, static_cast<uint32_t>(w.direction));
    Mix32(h, static_cast<uint32_t>(w.flags));
    Mix32(h, w.visible ? 1U : 0U);
    Mix32(h, w.ready ? 1U : 0U);
    Mix32(h, w.able_to_jump ? 1U : 0U);
    Mix32(h, w.able_to_dig ? 1U : 0U);
    Mix32(h, static_cast<uint32_t>(w.control_states.istate));
    Mix32(h, static_cast<uint32_t>(w.prev_control_states.istate));
    for (const auto& ww : w.weapons) {
      Mix32(h, static_cast<uint32_t>(ww.ammo));
      Mix32(h, static_cast<uint32_t>(ww.delay_left));
      Mix32(h, static_cast<uint32_t>(ww.loading_left));
    }
  }

  Mix32(h, static_cast<uint32_t>(game.wobjects.count));
  for (std::size_t i = 0; i < game.wobjects.count; ++i) {
    WObject const& o = game.wobjects.arr[i];
    Mix32(h, static_cast<uint32_t>(o.pos.x));
    Mix32(h, static_cast<uint32_t>(o.pos.y));
    Mix32(h, static_cast<uint32_t>(o.vel.x));
    Mix32(h, static_cast<uint32_t>(o.vel.y));
    Mix32(h, static_cast<uint32_t>(o.cur_frame));
    Mix32(h, static_cast<uint32_t>(o.time_left));
    Mix32(h, static_cast<uint32_t>(o.owner_idx));
  }
  Mix32(h, static_cast<uint32_t>(game.sobjects.count));
  for (std::size_t i = 0; i < game.sobjects.count; ++i) {
    SObject const& o = game.sobjects.arr[i];
    Mix32(h, static_cast<uint32_t>(o.x));
    Mix32(h, static_cast<uint32_t>(o.y));
    Mix32(h, static_cast<uint32_t>(o.cur_frame));
    Mix32(h, static_cast<uint32_t>(o.id));
  }
  Mix32(h, static_cast<uint32_t>(game.nobjects.count));
  for (std::size_t i = 0; i < game.nobjects.count; ++i) {
    NObject const& o = game.nobjects.arr[i];
    Mix32(h, static_cast<uint32_t>(o.pos.x));
    Mix32(h, static_cast<uint32_t>(o.pos.y));
    Mix32(h, static_cast<uint32_t>(o.vel.x));
    Mix32(h, static_cast<uint32_t>(o.vel.y));
    Mix32(h, static_cast<uint32_t>(o.cur_frame));
  }
  Mix32(h, static_cast<uint32_t>(game.bonuses.count));
  for (std::size_t i = 0; i < game.bonuses.count; ++i) {
    Bonus const& b = game.bonuses.arr[i];
    Mix32(h, static_cast<uint32_t>(b.x));
    Mix32(h, static_cast<uint32_t>(b.y));
    Mix32(h, static_cast<uint32_t>(b.frame));
    Mix32(h, static_cast<uint32_t>(b.timer));
  }

  std::size_t const kCells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
  if (kCells > 0) {
    MixBytes(h, game.level.data.data(), kCells);
  }

  return h;
}

bool ReplayReader::PlaybackFrame(Renderer& renderer) {
  Game& game = *this->game;

  bool settings_changed = false;

  while (true) {
    uint8_t const kFirst = reader.Get();

    if (kFirst == kReplayTagEmptyFrame) {
      break;
    }
    if (kFirst == kReplayTagSettings) {
      CerealRead(reader, *game.settings);
      settings_changed = true;
    } else if (kFirst == kReplayTagWormSettings) {
      uint32_t const kWormId = io::ReadUint32(reader);
      Worm const* w = game.WormByIdx(kWormId);
      if (w) {
        CerealRead(reader, *w->settings);
        settings_changed = true;
      }
    } else if (kFirst == kReplayTagEnd) {
      // End of replay
      return false;
    } else if (kFirst < kReplayTagEmptyFrame) {
      uint8_t state = kFirst;
      bool has_state = true;

      for (auto const& worm : game.worms) {
        if (!has_state)
          state = reader.Get();
        else
          has_state = false;

        worm->control_states.Unpack(state ^ worm->prev_control_states.Pack());
      }

      break;  // Read frame
    } else {
      throw io::ArchiveCheckError("Unexpected header byte");
    }
  }

  if (settings_changed) {
    game.UpdateSettings(renderer);
  }

  if ((game.cycles % (70 * 15)) == 0) {
    uint32_t const kExpected = io::ReadUint32(reader);
    uint32_t const kActual = WideRollbackChecksum(game);
    if (kActual != kExpected) throw io::ArchiveCheckError("Replay has desynced");
  }

  return true;
}

void ReplayWriter::RecordFrame() {
  Game& game = *this->game;

  if (settings_expired) {
    writer.Put(kReplayTagSettings);
    CerealWrite(writer, *game.settings);
    settings_expired = false;
  }

  bool write_states = false;

  if (game.worms.size() <= 3) {  // TODO: What limit do we want here? None?
    write_states = true;
  } else {
    for (auto const& worm : game.worms) {
      WormData& data = worm_data[worm.get()];
      if (worm->control_states != worm->prev_control_states) {
        write_states = true;
      }

      if (data.settings_expired) {
        writer.Put(kReplayTagWormSettings);
        io::WriteUint32(writer, worm->index);
        CerealWrite(writer, *worm->settings);
        data.settings_expired = false;
      }
    }
  }

  if (write_states) {
    for (auto const& worm : game.worms) {
      uint8_t const kState = worm->control_states.Pack() ^ worm->prev_control_states.Pack();

      assert(kState < kReplayTagEmptyFrame);

      writer.Put(kState);
    }
  } else {
    writer.Put(kReplayTagEmptyFrame);
  }

  if ((game.cycles % (70 * 15)) == 0) {
    uint32_t const kChecksum = WideRollbackChecksum(game);
    io::WriteUint32(writer, kChecksum);
  }
}

void ReplayWriter::Unfocus() {
  for (auto& [worm, data] : worm_data) data.last_settings_hash = worm->settings->UpdateHash();
  last_settings_hash = game->settings->UpdateHash();
}

void ReplayWriter::Focus() {
  for (auto& [worm, data] : worm_data) {
    if (data.last_settings_hash != worm->settings->UpdateHash()) data.settings_expired = true;
  }

  if (last_settings_hash != game->settings->UpdateHash()) settings_expired = true;
}
