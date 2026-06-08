#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

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
static std::string GetTcPath() {
  if (auto* p = std::getenv("TC_PATH")) return p;
  return "data/TC/openliero";
}

TEST_CASE("TC loads without errors", "[tc_load]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());

  REQUIRE_NOTHROW(common->load(kTcRoot));

  // Basic sanity checks on loaded data
  REQUIRE(!common->weapons.empty());
  REQUIRE(!common->nobject_types.empty());
  REQUIRE(!common->sobject_types.empty());
  REQUIRE(common->weap_order.size() == common->weapons.size());

  // Strings from tc.cfg should arrive as UTF-8. The Copyright string contains
  // 'ä' (U+00E4) which is two bytes in UTF-8 (0xC3 0xA4); historically tc.cfg
  // stored these as  (a control char), which the parser then yielded as
  // 0xC2 0x84 — neither of which decode to 'ä'. Anchor that here.
  std::string const& copy2 = common->s[SCopyright2];
  REQUIRE(copy2.find("Liero v1.33") == 0);
  REQUIRE(copy2.contains("\xC3\xA4"));   // 'ä' UTF-8
  REQUIRE(!copy2.contains("\xC2\x84"));  // bogus old form
  REQUIRE(common->GuessName() == "Liero v1.33");

  // Sound name lookup: known names resolve to a valid index that
  // round-trips back to the same name, unknown names yield -1.
  int const kSelectIdx = common->SoundIndex("select");
  REQUIRE(kSelectIdx >= 0);
  REQUIRE(common->sounds[kSelectIdx].name == "select");
  REQUIRE(common->SoundIndex("does_not_exist") == -1);

  // [sounds] hooks resolve via the loaded sound table.
  REQUIRE(common->sound_hook[SoundMenuSelect] == common->SoundIndex("select"));
  REQUIRE(common->sound_hook[SoundMenuMoveUp] == common->SoundIndex("moveup"));
  REQUIRE(common->sound_hook[SoundMenuMoveDown] == common->SoundIndex("movedown"));
  REQUIRE(common->sound_hook[SoundBump] == common->SoundIndex("bump"));
  REQUIRE(common->sound_hook[SoundBegin] == common->SoundIndex("begin"));
  REQUIRE(common->sound_hook[SoundReloaded] == common->SoundIndex("reloaded"));
  for (int const kI : common->sound_hook) REQUIRE(kI >= 0);

  // Step 5: sound fields in weapon / sobject configs are now name-typed.
  // Anchor a couple of known values so regressions in soundRefFromStr
  // (e.g. unknown-name returning 0 instead of -1) get caught.
  auto find_weapon = [&](std::string const& id) -> Weapon const& {
    for (auto& w : common->weapons)
      if (w.id_str == id) return w;
    FAIL("weapon not found: " + id);
    return common->weapons.front();
  };
  auto find_s_object = [&](std::string const& id) -> SObjectType const& {
    for (auto& s : common->sobject_types)
      if (s.id_str == id) return s;
    FAIL("sobject not found: " + id);
    return common->sobject_types.front();
  };
  REQUIRE(find_weapon("bazooka").launch_sound == common->SoundIndex("bazooka"));
  REQUIRE(find_weapon("bazooka").explo_sound == -1);
  REQUIRE(find_s_object("large_explosion").start_sound == common->SoundIndex("exp2"));
  REQUIRE(find_s_object("flashing_pixel").start_sound == -1);
}

TEST_CASE("weapon / sobject sound refs round-trip via save/load", "[tc_load]") {
  auto src = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  src->load(kTcRoot);

  for (auto const& w : src->weapons) {
    std::stringstream ss;
    SaveWeaponConfig(*src, w, ss);
    Weapon copy = w;
    copy.launch_sound = copy.loop_sound = copy.explo_sound = -999;
    LoadWeaponConfig(*src, copy, ss);
    REQUIRE(copy.launch_sound == w.launch_sound);
    REQUIRE(copy.loop_sound == w.loop_sound);
    REQUIRE(copy.explo_sound == w.explo_sound);
  }
  for (auto const& s : src->sobject_types) {
    std::stringstream ss;
    SaveSObjectConfig(*src, s, ss);
    SObjectType copy = s;
    copy.start_sound = -999;
    LoadSObjectConfig(*src, copy, ss);
    REQUIRE(copy.start_sound == s.start_sound);
  }
}

TEST_CASE("weapon cfg with unknown sound name resolves to -1, not 0", "[tc_load]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);

  // Round-trip a weapon, but rewrite exploSound to a bogus name. Must
  // resolve to -1 (no sound), NOT 0 (would spuriously play the first sound).
  Weapon const& src = common->weapons.front();
  std::stringstream out;
  SaveWeaponConfig(*common, src, out);
  std::string text = out.str();
  // Inject an unknown name into exploSound.
  auto pos = text.find("exploSound = ");
  REQUIRE(pos != std::string::npos);
  auto eol = text.find('\n', pos);
  text.replace(pos, eol - pos, "exploSound = \"definitely_not_a_sound\"");

  std::stringstream in(text);
  Weapon dst = src;
  LoadWeaponConfig(*common, dst, in);
  REQUIRE(dst.explo_sound == -1);
}

TEST_CASE("tc.cfg [sounds] round-trips through save/load", "[tc_load]") {
  auto src = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  src->load(kTcRoot);

  std::stringstream ss;
  SaveTcConfig(*src, ss);

  Common dst;
  // soundIndex resolution during load relies on `dst.sounds` being
  // populated from [types].sounds, which loadTcConfig does first.
  LoadTcConfig(dst, ss);

  for (int i = 0; i < SoundDefT::kMaxSound; ++i) REQUIRE(dst.sound_hook[i] == src->sound_hook[i]);
}

TEST_CASE("[sounds] unknown name resolves to -1", "[tc_load]") {
  std::string const kCfg =
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
  std::stringstream ss(kCfg);
  Common c;
  LoadTcConfig(c, ss);
  REQUIRE(c.sound_hook[SoundMenuSelect] == -1);
  REQUIRE(c.sound_hook[SoundMenuMoveUp] == c.SoundIndex("alpha"));
  // Unset entries default to -1.
  REQUIRE(c.sound_hook[SoundBump] == -1);
}

// Regression for issue #44: a TC whose [types].sounds lists a sound whose
// WAV file is missing on disk must still load — the slot survives with
// sound == nullptr, indices of later sounds are unchanged, and playing the
// missing slot is a silent no-op (not a crash, not the wrong sound).
TEST_CASE("TC with a missing sound WAV loads and keeps indices stable", "[tc_load][issue44]") {
  namespace fs = std::filesystem;
  fs::path const kTempTc = fs::temp_directory_path() / "openliero_test_missing_sound_tc";
  fs::remove_all(kTempTc);
  fs::copy(GetTcPath(), kTempTc, fs::copy_options::recursive | fs::copy_options::copy_symlinks);

  // Pick a sound that is NOT the last entry so we can verify later
  // indices don't shift. "shotgun" is sounds[0] in the shipped TC, and
  // "exp2" lives further down the array, referenced by the
  // "large_explosion" sobject.
  fs::path const kVictim = kTempTc / "sounds" / "shotgun.wav";
  REQUIRE(fs::exists(kVictim));
  fs::remove(kVictim);

  auto common = std::make_shared<Common>();
  REQUIRE_NOTHROW(common->load(FsNode(kTempTc.string())));

  int const kShotgunIdx = common->SoundIndex("shotgun");
  REQUIRE(kShotgunIdx >= 0);
  REQUIRE(common->sounds[kShotgunIdx].name == "shotgun");
  REQUIRE(common->sounds[kShotgunIdx].sound == nullptr);

  // Later entries must still resolve to the same (named) slot — the
  // shifting bug from issue #44 would have moved them up by one.
  int const kExp2Idx = common->SoundIndex("exp2");
  REQUIRE(kExp2Idx > kShotgunIdx);
  REQUIRE(common->sounds[kExp2Idx].name == "exp2");
  REQUIRE(common->sounds[kExp2Idx].sound != nullptr);

  // The sobject that plays "exp2" must point at the slot that's still
  // named "exp2", not at whatever happens to live at the shifted index.
  SObjectType const* large_exp = nullptr;
  for (auto const& s : common->sobject_types)
    if (s.id_str == "large_explosion") large_exp = &s;
  REQUIRE(large_exp != nullptr);
  REQUIRE(large_exp->start_sound == kExp2Idx);

  // Playing the missing slot must be a silent no-op, not a crash. Run
  // through the production play wrapper (NullSoundPlayer dispatches to
  // playImpl, which does nothing — but the wrapper still touches
  // Common::sounds[idx] on the SOUND_DEF_T overload path).
  NullSoundPlayer p;
  REQUIRE_NOTHROW(p.Play(kShotgunIdx));

  fs::remove_all(kTempTc);
}

TEST_CASE("TC supports game initialization", "[tc_load]") {
  PrecomputeTables();

  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(GetTcPath());
  common->load(kTcRoot);

  auto settings = std::make_shared<Settings>();
  settings->lives = 5;
  settings->loading_time = 0;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;

  // Clamp weapon selections to valid range for this TC
  int const kNumWeapons = static_cast<int>(common->weapons.size());
  for (auto& worm_setting : settings->worm_settings) {
    for (unsigned int& weapon : worm_setting->weapons) {
      if (std::cmp_greater(weapon, kNumWeapons)) weapon = 1;
    }
  }

  auto sp = std::make_shared<NullSoundPlayer>();
  Game game(common, settings, sp);
  game.rand.Seed(42);

  for (int idx = 0; idx < 2; ++idx) {
    auto w = std::make_shared<Worm>();
    w->settings = settings->worm_settings[idx];
    w->health = w->settings->health;
    w->index = idx;
    w->stats_x = idx == 0 ? 0 : 218;
    game.AddWorm(w);
  }

  game.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game.AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

  REQUIRE_NOTHROW(game.level.GenerateFromSettings(*common, *settings, game.rand));

  for (auto const& w : game.worms) REQUIRE_NOTHROW(w->InitWeapons(game));

  game.paused = false;
  game.StartGame();
  game.ResetWorms();

  // Run a short simulation — validates no crashes during gameplay
  Rand input_rng(12345);
  constexpr int kNumFrames = 200;

  for (int frame = 0; frame < kNumFrames; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t const kInput = input_rng() & 0x7f;
      game.worms[idx]->control_states.Unpack(kInput);
    }
    REQUIRE_NOTHROW(game.ProcessFrame());
  }
}
