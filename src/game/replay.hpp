#pragma once

#include <xxhash.h>
#include <cstring>
#include <map>
#include <memory>
#include "common.hpp"
#include "io/deflate.hpp"
#include "io/stream.hpp"
#include "mixer/player.hpp"
#include "version.hpp"
#include "worm.hpp"

struct Game;

struct Replay {
  Game* game = nullptr;
  int replay_version = kMyReplayVersion;
};

struct ReplayWriter : Replay {
  ReplayWriter(std::unique_ptr<io::Writer> sink);
  ~ReplayWriter();

  void Unfocus();
  void Focus();

  io::DeflateWriter writer;
  uint64_t last_settings_hash;
  bool settings_expired;

  struct WormData {
    WormData() : settings_expired(true) {}
    uint64_t last_settings_hash;
    bool settings_expired;
  };
  std::map<Worm*, WormData> worm_data;

  void BeginRecord(Game& game);
  void RecordFrame();

 private:
  void EndRecord();
};

struct Renderer;

struct ReplayReader : Replay {
  ReplayReader(std::unique_ptr<io::Reader> source);

  void Unfocus() {}
  void Focus() {}

  std::unique_ptr<Game> BeginPlayback(std::shared_ptr<Common> common,
                                      std::shared_ptr<SoundPlayer> sound_player);
  bool PlaybackFrame(Renderer& renderer);

  // The full inflated replay is held in memory so we can rewind to
  // the recorded initial position when the user presses R.
  std::vector<uint8_t> data;
  io::MemReader reader;
};
