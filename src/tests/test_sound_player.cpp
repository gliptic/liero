#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"

static std::string getTcPath() {
  if (auto* p = std::getenv("TC_PATH"))
    return p;
  return "data/TC/openliero";
}

namespace {

struct RecordingPlayer : SoundPlayer {
  RecordingPlayer() : m_common(nullptr) {}
  explicit RecordingPlayer(Common& c) : m_common(&c) {}

  std::vector<int> played;

  bool isPlaying(void* /*id*/) override { return false; }
  void stop(void* /*id*/) override {}

 protected:
  void playImpl(int sound, void* /*id*/, int /*loops*/) override {
    played.push_back(sound);
  }
  Common* common() override { return m_common; }

 private:
  Common* m_common;
};

}  // namespace

TEST_CASE("SoundPlayer guards negative indices", "[sound_player]") {
  RecordingPlayer p;
  p.play(-1);
  p.play(3);
  p.play(-5);
  p.play(0);

  REQUIRE(p.played.size() == 2);
  REQUIRE(p.played[0] == 3);
  REQUIRE(p.played[1] == 0);
}

TEST_CASE("SoundPlayer SOUND_DEF_T overload resolves via Common::soundHook",
          "[sound_player]") {
  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  common->load(std::move(tcRoot));

  RecordingPlayer p(*common);
  p.play(SoundMenuSelect);

  REQUIRE(p.played.size() == 1);
  REQUIRE(p.played[0] == common->soundHook[SoundMenuSelect]);
  REQUIRE(p.played[0] == common->soundIndex("select"));
}

TEST_CASE(
    "SoundPlayer SOUND_DEF_T overload is a no-op when common() returns null",
    "[sound_player]") {
  RecordingPlayer p;  // no Common attached
  p.play(SoundMenuSelect);
  REQUIRE(p.played.empty());
}

TEST_CASE("Game ctor installs g_soundPlayer and dtor restores it",
          "[sound_player]") {
  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  common->load(std::move(tcRoot));
  auto settings = std::make_shared<Settings>();

  SoundPlayer* originalGlobal = g_soundPlayer;
  auto sentinel = std::make_shared<NullSoundPlayer>();
  g_soundPlayer = sentinel.get();

  {
    auto sp = std::make_shared<RecordingPlayer>(*common);
    Game game(common, settings, sp);
    REQUIRE(g_soundPlayer == sp.get());
  }

  REQUIRE(g_soundPlayer == sentinel.get());

  g_soundPlayer = originalGlobal;
}
