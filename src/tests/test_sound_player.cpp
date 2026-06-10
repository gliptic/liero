#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "common.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "mixer/mixer.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"

static std::string GetTcPath() {
  if (auto* p = std::getenv("TC_PATH")) {
    return p;
  }
  return "data/TC/openliero";
}

namespace {

struct RecordingPlayer : SoundPlayer {
  RecordingPlayer() : m_common_(nullptr) {}
  explicit RecordingPlayer(Common& c) : m_common_(&c) {}

  std::vector<int> played;

  bool IsPlaying(void* /*id*/) override { return false; }
  void Stop(void* /*id*/) override {}

 protected:
  void PlayImpl(int sound, void* /*id*/, int /*loops*/) override { played.push_back(sound); }
  Common* GetCommonPtr() override { return m_common_; }

 private:
  Common* m_common_;
};

}  // namespace

TEST_CASE("SoundPlayer guards negative indices", "[sound_player]") {
  RecordingPlayer p;
  p.Play(-1);
  p.Play(3);
  p.Play(-5);
  p.Play(0);

  REQUIRE(p.played.size() == 2);
  REQUIRE(p.played[0] == 3);
  REQUIRE(p.played[1] == 0);
}

TEST_CASE("SoundPlayer SOUND_DEF_T overload resolves via Common::soundHook", "[sound_player]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);

  RecordingPlayer p(*common);
  p.Play(SoundMenuSelect);

  REQUIRE(p.played.size() == 1);
  REQUIRE(p.played[0] == common->sound_hook[SoundMenuSelect]);
  REQUIRE(p.played[0] == common->SoundIndex("select"));
}

TEST_CASE("SoundPlayer SOUND_DEF_T overload is a no-op when common() returns null",
          "[sound_player]") {
  RecordingPlayer p;  // no Common attached
  p.Play(SoundMenuSelect);
  REQUIRE(p.played.empty());
}

TEST_CASE("Stop passes through while speculative", "[sound_player]") {
  // Regression: stop() used to be suppressed while speculative. A loop
  // sound whose stop lands only on speculative frames then plays
  // forever, permanently occupying one of the CHANNEL_COUNT mixer
  // channels — leak enough of them and every later play() (game and
  // menu alike) silently fails.
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);

  int snd = -1;
  for (std::size_t i = 0; i < common->sounds.size(); ++i) {
    if (common->sounds[i].sound) {
      snd = static_cast<int>(i);
      break;
    }
  }
  REQUIRE(snd >= 0);

  sfx_mixer* mixer = SfxMixerCreate();
  RecordSoundPlayer p(*common, mixer);

  int channel_id = 0;
  p.Play(snd, &channel_id, /*loops=*/-1);
  REQUIRE(p.IsPlaying(&channel_id));

  p.speculative = true;
  p.Stop(&channel_id);
  REQUIRE_FALSE(p.IsPlaying(&channel_id));
}

TEST_CASE("Game ctor can leave g_soundPlayer alone for shadow games", "[sound_player]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);
  auto settings = std::make_shared<Settings>();

  SoundPlayer* original_global = g_sound_player;
  auto sentinel = std::make_shared<NullSoundPlayer>();
  g_sound_player = sentinel.get();

  {
    auto sp = std::make_shared<RecordingPlayer>(*common);
    Game const kGame(common, settings, sp, /*install_global_sound_player=*/false);
    REQUIRE(g_sound_player == sentinel.get());
  }

  REQUIRE(g_sound_player == sentinel.get());

  g_sound_player = original_global;
}

TEST_CASE("Game ctor installs g_soundPlayer and dtor restores it", "[sound_player]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);
  auto settings = std::make_shared<Settings>();

  SoundPlayer* original_global = g_sound_player;
  auto sentinel = std::make_shared<NullSoundPlayer>();
  g_sound_player = sentinel.get();

  {
    auto sp = std::make_shared<RecordingPlayer>(*common);
    Game const kGame(common, settings, sp);
    REQUIRE(g_sound_player == sp.get());
  }

  REQUIRE(g_sound_player == sentinel.get());

  g_sound_player = original_global;
}
