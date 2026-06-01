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
  int replayVersion = myReplayVersion;
};

struct ReplayWriter : Replay {
  ReplayWriter(std::unique_ptr<io::Writer> sink);
  ~ReplayWriter();

  void unfocus();
  void focus();

  io::DeflateWriter writer;
  uint64_t lastSettingsHash;
  bool settingsExpired;

  struct WormData {
    WormData() : settingsExpired(true) {}
    uint64_t lastSettingsHash;
    bool settingsExpired;
  };
  std::map<Worm*, WormData> wormData;

  void beginRecord(Game& game);
  void recordFrame();

 private:
  void endRecord();
};

struct Renderer;

struct ReplayReader : Replay {
  ReplayReader(std::unique_ptr<io::Reader> source);

  void unfocus() {}
  void focus() {}

  std::unique_ptr<Game> beginPlayback(std::shared_ptr<Common> common,
                                      std::shared_ptr<SoundPlayer> soundPlayer);
  bool playbackFrame(Renderer& renderer);

  // The full inflated replay is held in memory so we can rewind to
  // the recorded initial position when the user presses R.
  std::vector<uint8_t> data;
  io::MemReader reader;
};
