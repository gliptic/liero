#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "common.hpp"
#include "common_model.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "viewport.hpp"
#include "worm.hpp"

// TC path can be overridden via TC_PATH env var (default: data/TC/openliero)
static std::string getTcPath() {
  if (auto* p = std::getenv("TC_PATH"))
    return p;
  return "data/TC/openliero";
}

TEST_CASE("TC loads without errors", "[tc_load]") {
  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());

  REQUIRE_NOTHROW(common->load(std::move(tcRoot)));

  // Basic sanity checks on loaded data
  REQUIRE(common->weapons.size() > 0);
  REQUIRE(common->nobjectTypes.size() > 0);
  REQUIRE(common->sobjectTypes.size() > 0);
  REQUIRE(common->weapOrder.size() == common->weapons.size());

  // Strings from tc.cfg should arrive as UTF-8. The Copyright string contains
  // 'ä' (U+00E4) which is two bytes in UTF-8 (0xC3 0xA4); historically tc.cfg
  // stored these as  (a control char), which the parser then yielded as
  // 0xC2 0x84 — neither of which decode to 'ä'. Anchor that here.
  std::string const& copy2 = common->S[SCopyright2];
  REQUIRE(copy2.find("Liero v1.33") == 0);
  REQUIRE(copy2.find("\xC3\xA4") != std::string::npos);  // 'ä' UTF-8
  REQUIRE(copy2.find("\xC2\x84") == std::string::npos);  // bogus old form
  REQUIRE(common->guessName() == "Liero v1.33");

  // Sound name lookup: known names resolve to a valid index that
  // round-trips back to the same name, unknown names yield -1.
  int selectIdx = common->soundIndex("select");
  REQUIRE(selectIdx >= 0);
  REQUIRE(common->sounds[selectIdx].name == "select");
  REQUIRE(common->soundIndex("does_not_exist") == -1);

  // [sounds] hooks resolve via the loaded sound table.
  REQUIRE(common->soundHook[SoundMenuSelect] == common->soundIndex("select"));
  REQUIRE(
      common->soundHook[SoundMenuMoveUp] == common->soundIndex("moveup"));
  REQUIRE(
      common->soundHook[SoundMenuMoveDown] == common->soundIndex("movedown"));
  REQUIRE(common->soundHook[SoundBump] == common->soundIndex("bump"));
  REQUIRE(common->soundHook[SoundBegin] == common->soundIndex("begin"));
  REQUIRE(common->soundHook[SoundReloaded] == common->soundIndex("reloaded"));
  for (int i = 0; i < SOUND_DEF_T::MaxSound; ++i)
    REQUIRE(common->soundHook[i] >= 0);

  // Step 5: sound fields in weapon / sobject configs are now name-typed.
  // Anchor a couple of known values so regressions in soundRefFromStr
  // (e.g. unknown-name returning 0 instead of -1) get caught.
  auto findWeapon = [&](std::string const& id) -> Weapon const& {
    for (auto& w : common->weapons)
      if (w.idStr == id)
        return w;
    FAIL("weapon not found: " + id);
    return common->weapons.front();
  };
  auto findSObject = [&](std::string const& id) -> SObjectType const& {
    for (auto& s : common->sobjectTypes)
      if (s.idStr == id)
        return s;
    FAIL("sobject not found: " + id);
    return common->sobjectTypes.front();
  };
  REQUIRE(findWeapon("bazooka").launchSound == common->soundIndex("bazooka"));
  REQUIRE(findWeapon("bazooka").exploSound == -1);
  REQUIRE(findSObject("large_explosion").startSound == common->soundIndex("exp2"));
  REQUIRE(findSObject("flashing_pixel").startSound == -1);
}

TEST_CASE("weapon / sobject sound refs round-trip via save/load", "[tc_load]") {
  auto src = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  src->load(std::move(tcRoot));

  for (auto const& w : src->weapons) {
    std::stringstream ss;
    saveWeaponConfig(*src, w, ss);
    Weapon copy = w;
    copy.launchSound = copy.loopSound = copy.exploSound = -999;
    loadWeaponConfig(*src, copy, ss);
    REQUIRE(copy.launchSound == w.launchSound);
    REQUIRE(copy.loopSound == w.loopSound);
    REQUIRE(copy.exploSound == w.exploSound);
  }
  for (auto const& s : src->sobjectTypes) {
    std::stringstream ss;
    saveSObjectConfig(*src, s, ss);
    SObjectType copy = s;
    copy.startSound = -999;
    loadSObjectConfig(*src, copy, ss);
    REQUIRE(copy.startSound == s.startSound);
  }
}

TEST_CASE(
    "weapon cfg with unknown sound name resolves to -1, not 0", "[tc_load]") {
  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  common->load(std::move(tcRoot));

  // Round-trip a weapon, but rewrite exploSound to a bogus name. Must
  // resolve to -1 (no sound), NOT 0 (would spuriously play the first sound).
  Weapon const& src = common->weapons.front();
  std::stringstream out;
  saveWeaponConfig(*common, src, out);
  std::string text = out.str();
  // Inject an unknown name into exploSound.
  auto pos = text.find("exploSound = ");
  REQUIRE(pos != std::string::npos);
  auto eol = text.find('\n', pos);
  text.replace(pos, eol - pos, "exploSound = \"definitely_not_a_sound\"");

  std::stringstream in(text);
  Weapon dst = src;
  loadWeaponConfig(*common, dst, in);
  REQUIRE(dst.exploSound == -1);
}

TEST_CASE("tc.cfg [sounds] round-trips through save/load", "[tc_load]") {
  auto src = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  src->load(std::move(tcRoot));

  std::stringstream ss;
  saveTcConfig(*src, ss);

  Common dst;
  // soundIndex resolution during load relies on `dst.sounds` being
  // populated from [types].sounds, which loadTcConfig does first.
  loadTcConfig(dst, ss);

  for (int i = 0; i < SOUND_DEF_T::MaxSound; ++i)
    REQUIRE(dst.soundHook[i] == src->soundHook[i]);
}

TEST_CASE("[sounds] unknown name resolves to -1", "[tc_load]") {
  std::string cfg =
      "[types]\n"
      "sounds = [\"alpha\", \"beta\"]\n"
      "weapons = []\n"
      "nobjects = []\n"
      "sobjects = []\n"
      "\n"
      "[constants]\n"
      "materials = []\n"
      "[hacks]\n"
      "[texts]\n"
      "[sounds]\n"
      "MenuSelect = \"not_a_real_sound\"\n"
      "MenuMoveUp = \"alpha\"\n";
  std::stringstream ss(cfg);
  Common c;
  loadTcConfig(c, ss);
  REQUIRE(c.soundHook[SoundMenuSelect] == -1);
  REQUIRE(c.soundHook[SoundMenuMoveUp] == c.soundIndex("alpha"));
  // Unset entries default to -1.
  REQUIRE(c.soundHook[SoundBump] == -1);
}

// Regression for issue #44: a TC whose [types].sounds lists a sound whose
// WAV file is missing on disk must still load — the slot survives with
// sound == nullptr, indices of later sounds are unchanged, and playing the
// missing slot is a silent no-op (not a crash, not the wrong sound).
TEST_CASE("TC with a missing sound WAV loads and keeps indices stable",
          "[tc_load][issue44]") {
  namespace fs = std::filesystem;
  fs::path const tempTc =
      fs::temp_directory_path() / "openliero_test_missing_sound_tc";
  fs::remove_all(tempTc);
  fs::copy(
      getTcPath(), tempTc,
      fs::copy_options::recursive | fs::copy_options::copy_symlinks);

  // Pick a sound that is NOT the last entry so we can verify later
  // indices don't shift. "shotgun" is sounds[0] in the shipped TC, and
  // "exp2" lives further down the array, referenced by the
  // "large_explosion" sobject.
  fs::path const victim = tempTc / "sounds" / "shotgun.wav";
  REQUIRE(fs::exists(victim));
  fs::remove(victim);

  auto common = std::make_shared<Common>();
  REQUIRE_NOTHROW(common->load(FsNode(tempTc.string())));

  int const shotgunIdx = common->soundIndex("shotgun");
  REQUIRE(shotgunIdx >= 0);
  REQUIRE(common->sounds[shotgunIdx].name == "shotgun");
  REQUIRE(common->sounds[shotgunIdx].sound == nullptr);

  // Later entries must still resolve to the same (named) slot — the
  // shifting bug from issue #44 would have moved them up by one.
  int const exp2Idx = common->soundIndex("exp2");
  REQUIRE(exp2Idx > shotgunIdx);
  REQUIRE(common->sounds[exp2Idx].name == "exp2");
  REQUIRE(common->sounds[exp2Idx].sound != nullptr);

  // The sobject that plays "exp2" must point at the slot that's still
  // named "exp2", not at whatever happens to live at the shifted index.
  SObjectType const* largeExp = nullptr;
  for (auto const& s : common->sobjectTypes)
    if (s.idStr == "large_explosion")
      largeExp = &s;
  REQUIRE(largeExp != nullptr);
  REQUIRE(largeExp->startSound == exp2Idx);

  // Playing the missing slot must be a silent no-op, not a crash. Run
  // through the production play wrapper (NullSoundPlayer dispatches to
  // playImpl, which does nothing — but the wrapper still touches
  // Common::sounds[idx] on the SOUND_DEF_T overload path).
  NullSoundPlayer p;
  REQUIRE_NOTHROW(p.play(shotgunIdx));

  fs::remove_all(tempTc);
}

TEST_CASE("TC supports game initialization", "[tc_load]") {
  precomputeTables();

  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  common->load(std::move(tcRoot));

  auto settings = std::make_shared<Settings>();
  settings->lives = 5;
  settings->loadingTime = 0;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;

  // Clamp weapon selections to valid range for this TC
  int numWeapons = (int)common->weapons.size();
  for (int p = 0; p < 3; ++p) {
    for (int i = 0; i < Settings::selectableWeapons; ++i) {
      if ((int)settings->wormSettings[p]->weapons[i] > numWeapons)
        settings->wormSettings[p]->weapons[i] = 1;
    }
  }

  auto sp = std::make_shared<NullSoundPlayer>();
  Game game(common, settings, sp);
  game.rand.seed(42);

  for (int idx = 0; idx < 2; ++idx) {
    auto w = std::make_shared<Worm>();
    w->settings = settings->wormSettings[idx];
    w->health = w->settings->health;
    w->index = idx;
    w->statsX = idx == 0 ? 0 : 218;
    game.addWorm(w);
  }

  game.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

  REQUIRE_NOTHROW(game.level.generateFromSettings(*common, *settings, game.rand));

  for (auto const& w : game.worms)
    REQUIRE_NOTHROW(w->initWeapons(game));

  game.paused = false;
  game.startGame();
  game.resetWorms();

  // Run a short simulation — validates no crashes during gameplay
  Rand inputRng(12345);
  constexpr int NUM_FRAMES = 200;

  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;
      game.worms[idx]->controlStates.unpack(input);
    }
    REQUIRE_NOTHROW(game.processFrame());
  }
}
