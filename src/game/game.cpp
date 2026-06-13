#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#include "ai/predictive_ai.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "keys.hpp"
#include "serialization/fast_snapshot.hpp"
#include "serialization/snapshot.hpp"
#include "spectatorviewport.hpp"
#include "viewport.hpp"
#include "weapsel.hpp"
#include "worm.hpp"

#include <cereal/archives/portable_binary.hpp>
#include <memory>
#include <sstream>
#include <utility>

Game::Game(const std::shared_ptr<Common>& common, std::shared_ptr<Settings> settings_init,
           const std::shared_ptr<SoundPlayer>& sound_player, bool install_global_sound_player)
    : common(common),
      sound_player(sound_player),
      prev_sound_player(g_sound_player),
      sound_player_installed(install_global_sound_player),

      settings(std::move(std::move(settings_init))),
      stats_recorder(new NormalStatsRecorder),
      level(*common)

{
  if (install_global_sound_player) {
    g_sound_player = sound_player.get();
  }

  rand.Seed(static_cast<uint32_t>(std::time(nullptr)));
}

Game::~Game() {
  ClearViewports();
  ClearWorms();
  // The player may be shared (gfx.soundPlayer); never leave it muted
  // past this game's lifetime.
  if (sound_player) {
    sound_player->speculative = false;
  }
  if (sound_player_installed && g_sound_player == sound_player.get()) {
    g_sound_player = prev_sound_player;
  }
}

void Game::OnKey(uint32_t key, bool state) {
  for (auto& worm : worms) {
    Worm& w = *worm;

    // Only check keyboard controls for players using keyboard input
    if (w.settings->input_device != WormSettingsExtensions::kInputKeyboard) {
      continue;
    }

    for (std::size_t control = 0; control < WormSettings::kMaxControl; ++control) {
      if (w.settings->controls[control] == key) {
        w.SetControlState(static_cast<Worm::Control>(control), state);
      }
    }
  }
}

Worm* Game::FindControlForKey(uint32_t key, Worm::Control& control) {
  // Gamepad control keys encode the player index and control directly
  if (IsGamepadControlKey(key)) {
    int const kPlayerIdx = (key - kGamepadControlKeysStart) / 8;
    int const kC = (key - kGamepadControlKeysStart) % 8;
    if (kPlayerIdx >= 0 && std::cmp_less(kPlayerIdx, worms.size())) {
      control = static_cast<Worm::Control>(kC);
      return worms[kPlayerIdx].get();
    }
    return nullptr;
  }

  for (auto& worm : worms) {
    Worm& w = *worm;

    // Only check keyboard bindings for players using keyboard
    if (w.settings->input_device != WormSettingsExtensions::kInputKeyboard) {
      continue;
    }

    uint32_t const* controls =
        Settings::kExtensions ? w.settings->controls_ex : w.settings->controls;
    std::size_t const kMaxControl =
        Settings::kExtensions ? WormSettings::kMaxControlEx : WormSettings::kMaxControl;
    for (std::size_t c = 0; c < kMaxControl; ++c) {
      if (controls[c] == key) {
        control = static_cast<Worm::Control>(c);
        return &w;
      }
    }
  }

  return nullptr;
}

void Game::ReleaseControls() {
  for (auto& worm : worms) {
    Worm& w = *worm;

    for (std::size_t control = 0; control < WormSettings::kMaxControl; ++control) {
      w.Release(static_cast<Worm::Control>(control));
    }
  }
}

void Game::ClearViewports() {
  viewports.clear();
  spectator_viewports.clear();
}

void Game::AddViewport(Viewport* vp) {
  // vp->worm->viewport = vp;
  viewports.push_back(vp);
}

void Game::AddSpectatorViewport(SpectatorViewport* vp) { spectator_viewports.push_back(vp); }

void Game::ProcessViewports() {
  for (auto& viewport : viewports) {
    viewport->Process(*this);
  }
  for (auto& spectator_viewport : spectator_viewports) {
    spectator_viewport->Process(*this);
  }
}

void Game::DrawViewports(Renderer& renderer, GameState state, bool is_replay) {
  for (auto& viewport : viewports) {
    viewport->Draw(*this, renderer, state, is_replay);
  }
}

void Game::DrawSpectatorViewports(Renderer& renderer, GameState state, bool is_replay) {
  for (auto& spectator_viewport : spectator_viewports) {
    spectator_viewport->Draw(*this, renderer, state, is_replay);
  }
}

void Game::ClearWorms() { worms.clear(); }

void Game::ResetWorms() {
  for (auto& worm : worms) {
    Worm& w = *worm;
    w.health = w.settings->health;
    w.lives = settings->lives;  // Not in the original!
    w.kills = 0;
    w.visible = false;
    w.killed_timer = Worm::kKilledTimerInitial;

    w.current_weapon = 0;
  }
}

void Game::AddWorm(std::shared_ptr<Worm> worm) { worms.push_back(std::move(worm)); }

void Game::Draw(Renderer& renderer, GameState state, bool use_spectator_viewports, bool is_replay) {
  // Finalize the palette before drawing: blits resolve through pal32 at
  // draw time, so the rebuild must precede everything drawn this frame.
  renderer.pal = renderer.Origpal();

  for (auto& w : common->color_anim) {
    renderer.pal.RotateFrom(renderer.Origpal(), w.from, w.to, cycles >> 3);
  }

  if (screen_flash > 0) {
    renderer.pal.LightUp(screen_flash);
  }

  renderer.UpdatePal32();

  // Repaint the background through the freshly rebuilt palette. Callers
  // clear before drawing, but that resolves entry 0 through the previous
  // frame's LUT — visible as a one-frame lag on the border pixels while a
  // screen flash lights up the palette.
  Fill(renderer.bmp, 0);

  if (use_spectator_viewports) {
    DrawSpectatorViewports(renderer, state, is_replay);
  } else {
    DrawViewports(renderer, state, is_replay);
  }

  // common->font.drawText(toString(cycles / 70), 10, 10, 7);
}

bool CheckBonusSpawnPosition(Game& game, int x, int y) {
  Rect rect(x - 2, y - 2, x + 3, y + 3);

  rect.Intersect(game.level.Bounds());

  for (int cx = rect.x1; cx < rect.x2; ++cx) {
    for (int cy = rect.y1; cy < rect.y2; ++cy) {
      if (game.level.Mat(cx, cy).DirtRock()) {
        return false;
      }
    }
  }

  return true;
}

void Game::CreateBonus() {
  Common& common = *this->common;

  if (std::cmp_greater_equal(bonuses.Size(), settings->max_bonuses)) {
    return;
  }

  for (std::size_t i = 0; i < 50000; ++i) {
    int ix = rand(LC(BonusSpawnRectW));
    int iy = rand(LC(BonusSpawnRectH));

    if (common.h[HBonusSpawnRect]) {
      ix += LC(BonusSpawnRectX);
      iy += LC(BonusSpawnRectY);
    }

    if (CheckBonusSpawnPosition(*this, ix, iy)) {
      int frame = 0;

      if (common.h[HBonusOnlyHealth]) {
        frame = 1;
      } else if (common.h[HBonusOnlyWeapon]) {
        frame = 0;
      } else {
        frame = rand(2);
      }

      Bonus* bonus = bonuses.NewObject();
      if (!bonus) {
        return;
      }

      bonus->x = Itof(ix);
      bonus->y = Itof(iy);
      bonus->vel_y = 0;
      bonus->frame = frame;
      bonus->timer = rand(common.bonus_rand_timer[frame][1]) + common.bonus_rand_timer[frame][0];
      bonus->weapon = 0;

      if (frame == 0) {
        do {
          bonus->weapon = rand(static_cast<uint32_t>(common.weapons.size()));
        } while (settings->weap_table[bonus->weapon] == 2);
      }

      common.sobject_types[7].Create(*this, ix, iy, 0, nullptr);
      return;
    }
  }  // 234F
}

void Game::ProcessFrame() {
  stats_recorder->PreTick(*this);

  if (screen_flash > 0) {
    --screen_flash;
  }

  for (auto& viewport : viewports) {
    if (viewport->shake > 0) {
      viewport->shake -= 4000;  // TODO: Read 4000 from exe?
    }
  }

  for (auto& spectator_viewport : spectator_viewports) {
    if (spectator_viewport->shake > 0) {
      spectator_viewport->shake -= 4000;  // TODO: Read 4000 from exe?
    }
  }

  auto br = bonuses.All();
  for (Bonus* i = nullptr; (i = br.Next());) {
    i->Process(*this);
  }

  if ((cycles & 1) == 0) {
    for (auto& viewport : viewports) {
      Viewport& v = *viewport;

      bool down = false;

      if (WormByIdx(v.worm_idx)->killed_timer > 16) {
        down = true;
      }

      if (down) {
        if (v.banner_y < 2) {
          ++v.banner_y;
        }
      } else {
        if (v.banner_y > -8) {
          --v.banner_y;
        }
      }
    }
    // FIXME duplicated code
    for (auto& spectator_viewport : spectator_viewports) {
      SpectatorViewport& v = *spectator_viewport;

      bool down = false;

      if (WormByIdx(0)->killed_timer > 16 || WormByIdx(1)->killed_timer > 16) {
        down = true;
      }

      if (down) {
        if (v.banner_y < 2) {
          ++v.banner_y;
        }
      } else {
        if (v.banner_y > -8) {
          --v.banner_y;
        }
      }
    }
  }

  auto sr = sobjects.All();
  for (SObject* i = nullptr; (i = sr.Next());) {
    i->Process(*this);
  }

  auto wr = wobjects.All();
  for (WObject* i = nullptr; (i = wr.Next());) {
    i->Process(*this);
  }

  auto nr = nobjects.All();
  for (NObject* i = nullptr; (i = nr.Next());) {
    i->Process(*this);
  }

  for (BObjectList::Iterator i = bobjects.Begin(); i != bobjects.End();) {
    if (i->Process(*this)) {
      ++i;
    } else {
      bobjects.Free(i);
    }
  }

  // NOTE: This was originally the beginning of the processing, but has been rotated down to
  // separate out the drawing
  ++cycles;

  if (!common->h[HBonusDisable] && settings->max_bonuses > 0 &&
      rand(common->c[CBonusDropChance]) == 0) {
    CreateBonus();
  }

  for (auto& worm : worms) {
    worm->Process(*this);
  }

  for (auto& worm : worms) {
    worm->ninjarope.Process(*worm, *this);
  }

  switch (settings->game_mode) {
    case Settings::kGmGameOfTag: {
      bool some_invisible = false;
      for (auto& worm : worms) {
        if (!worm->visible) {
          some_invisible = true;
          break;
        }
      }

      Worm* last_killed_by = WormByIdx(last_killed_idx);

      if (!some_invisible && last_killed_by && (cycles % 70) == 0 &&
          last_killed_by->timer < settings->time_to_lose) {
        ++last_killed_by->timer;
      }
    } break;

    case Settings::kGmHoldazone: {
      int contender_idx = -1;
      int contenders = 0;

      for (auto const& w : worms) {
        int const kX = Ftoi(w->pos.x);
        int const kY = Ftoi(w->pos.y);

        if (w->visible && holdazone.rect.Inside(kX, kY)) {
          contender_idx = w->index;
          ++contenders;
        }
      }

      if (contenders == 0) {
        contender_idx = holdazone.holder_idx;
      }

      if (contenders <= 1) {
        if (contender_idx < 0 ||
            (holdazone.contender_idx != contender_idx && holdazone.contender_frames != 0)) {
          // NOLINTNEXTLINE(bugprone-inc-dec-in-conditions) — short-circuit-then-mutate is the entire point: only decrement when not already 0.
          if (holdazone.contender_frames == 0 || --holdazone.contender_frames == 0) {
            holdazone.contender_idx = contender_idx;
            holdazone.holder_idx = -1;
          }
        } else {
          holdazone.contender_idx = contender_idx;

          // NOLINTBEGIN(bugprone-inc-dec-in-conditions) — guarded increment: only fire on the exact frame that crosses the capture threshold.
          if (holdazone.contender_frames < Settings::kZoneCaptureTime &&
              ++holdazone.contender_frames >= Settings::kZoneCaptureTime &&
              holdazone.holder_idx != holdazone.contender_idx) {
            // NOLINTEND(bugprone-inc-dec-in-conditions) New holder

            int new_timeout = holdazone.timeout_left;
            if (holdazone.contender_idx >= 0) {
              new_timeout += settings->zone_timeout * 70 / 4;
            } else {
              new_timeout += settings->zone_timeout * 70 / 8;
            }

            holdazone.timeout_left = std::min(new_timeout, settings->zone_timeout * 70);

            holdazone.holder_idx = holdazone.contender_idx;
          }
        }
      }

      bool dec = false;

      if (holdazone.holder_idx >= 0) {
        auto* holder = WormByIdx(holdazone.holder_idx);

        if ((cycles % 70) == 0) {
          ++holder->timer;
        }

        dec = true;
      } else {
        dec = (cycles % 4) == 0;
      }

      if (dec) {
        if (--holdazone.timeout_left <= 0) {
          SpawnZone();
        }
      }
    } break;
    default:
      break;
  }

  ProcessViewports();

  // Store old control states so we can see what changes (mainly for replays)
  for (auto& worm : worms) {
    worm->prev_control_states = worm->control_states;
  }

  stats_recorder->Tick(*this);
}

void Game::Focus(Renderer& renderer) { UpdateSettings(renderer); }

void Game::UpdateSettings(Renderer& renderer) {
  renderer.origpal = level.origpal;  // Activate the Level palette
  // A level's custom palette wins in both modes; the modern palette only
  // replaces the stock one.
  renderer.origpal_modern = level.has_custom_palette ? level.origpal : common->modernpal;

  for (auto& i : worms) {
    Worm const& worm = *i;
    if (worm.index >= 0 && worm.index < 2) {
      renderer.origpal.SetWormColour(worm.index, *worm.settings, ColorMode::kClassic);
      renderer.origpal_modern.SetWormColour(worm.index, *worm.settings, ColorMode::kModern);
    }
  }
}

void Game::SpawnZone() {
  IVec2 pos;

  while (holdazone.zone_width >= 5) {
    if (level.SelectSpawn(rand, holdazone.zone_width, holdazone.zone_height - 8, pos)) {
      holdazone.rect.x1 = pos.x;
      holdazone.rect.y1 = pos.y;
      holdazone.rect.x2 = pos.x + holdazone.zone_width;
      holdazone.rect.y2 = pos.y + holdazone.zone_height;
      holdazone.timeout_left = settings->zone_timeout * 70;
      holdazone.contender_idx = -1;
      holdazone.contender_frames = 0;
      holdazone.holder_idx = -1;
      break;
    }

    holdazone.zone_width /= 2;
    holdazone.zone_height /= 2;
  }
}

void Game::StartGame() {
  sound_player->Play(common->sound_hook[SoundBegin]);
  bobjects.Resize(settings->blood_particle_max);
  stats_recorder->Reset(level.width, level.height);

  if (settings->game_mode == Settings::kGmHoldazone) {
    SpawnZone();
  }
}

bool Game::IsGameOver() {
  if (settings->game_mode == Settings::kGmKillEmAll ||
      settings->game_mode == Settings::kGmScalesOfJustice) {
    for (auto& worm : worms) {
      if (worm->lives <= 0) {
        return true;
      }
    }
  } else if (settings->game_mode == Settings::kGmGameOfTag) {
    for (auto& worm : worms) {
      if (worm->timer >= settings->time_to_lose) {
        return true;
      }
    }
  } else if (settings->game_mode == Settings::kGmHoldazone) {
    for (auto const& w : worms) {
      if (w->timer >= settings->time_to_lose) {
        return true;
      }
    }
  }

  return false;
}

void Game::DoDamageDirect(Worm& w, int amount, int by_idx) {
  if (amount > 0) {
    w.health -= amount;
    if (w.health <= 0) {
      w.last_killed_by_idx = by_idx;
    }
  }
}

void Game::DoHealingDirect(Worm& w, int amount) {
  w.health += amount;
  if (settings->game_mode == Settings::kGmScalesOfJustice) {
    while (w.health > w.settings->health) {
      w.lives += 1;
      w.health -= w.settings->health;
    }
  } else {
    w.health = std::min(w.health, w.settings->health);
  }
}

void Game::DoDamage(Worm& w, int amount, int by_idx) {
  DoDamageDirect(w, amount, by_idx);

  if (amount > 0) {
    if (settings->game_mode == Settings::kGmScalesOfJustice) {
      if (by_idx < 0 || by_idx == w.index) {
        int parts = static_cast<int>(worms.size()) - 1;
        int left = amount;

        for (auto const& other : worms) {
          if (other.get() != &w) {
            int const k_ = left / parts;
            DoHealingDirect(*other, k_);
            parts -= 1;
            left -= k_;
          }
        }
      } else {
        DoHealingDirect(*worms[by_idx], amount);
      }
    }
  }
}

void Game::DoHealing(Worm& w, int amount) {
  DoHealingDirect(w, amount);

  if (settings->game_mode == Settings::kGmScalesOfJustice) {
    int parts = static_cast<int>(worms.size()) - 1;
    int left = amount;

    for (auto const& other : worms) {
      if (other.get() != &w) {
        int const k_ = left / parts;
        DoDamageDirect(*other, k_, w.index);
        parts -= 1;
        left -= k_;
      }
    }
  } else {
    w.health = std::min(w.health, w.settings->health);
  }
}

bool CheckRespawnPosition(Game& game, int x2, int y2, int old_x, int old_y, int x, int y) {
  Common const& common = *game.common;

  int const kDeltaX = old_x;
  int const kDeltaY = old_y - y;
  int const kEnemyDx = x2 - x;
  int const kEnemyDy = y2 - y;

  if ((std::abs(kDeltaX) <= LC(WormMinSpawnDistLast) &&
       std::abs(kDeltaY) <= LC(WormMinSpawnDistLast)) ||
      (std::abs(kEnemyDx) <= LC(WormMinSpawnDistEnemy) &&
       std::abs(kEnemyDy) <= LC(WormMinSpawnDistEnemy))) {
    return false;
  }

  int max_x = x + 3;
  int max_y = y + 4;
  int min_x = x - 3;
  int min_y = y - 4;

  if (max_x >= game.level.width) {
    max_x = game.level.width - 1;
  }
  if (max_y >= game.level.height) {
    max_y = game.level.height - 1;
  }
  min_x = std::max(min_x, 0);
  min_y = std::max(min_y, 0);

  for (int i = min_x; i != max_x; ++i) {
    for (int j = min_y; j != max_y; ++j) {
      if (game.level.Mat(i, j).Rock()) {  // TODO: The special rock respawn bug is here, consider an
                                          // option to turn it off
        return false;
      }
    }
  }

  return true;
}

void Game::PostClone(Game& /*original*/, bool complete) {
  sound_player_installed = false;
  prev_sound_player = nullptr;
  if (!complete) {
    stats_recorder = std::make_shared<StatsRecorder>();
    sound_player = std::make_shared<NullSoundPlayer>();
    viewports.clear();
  } else {
    stats_recorder =
        std::make_shared<NormalStatsRecorder>(dynamic_cast<NormalStatsRecorder&>(*stats_recorder));

    for (auto& vp : viewports) {
      vp = new Viewport(*vp);
    }
  }

  for (auto& w : worms) {
    w = std::make_shared<Worm>(*w);
  }
}

void Game::SaveSnapshot(std::vector<uint8_t>& out) const {
  std::ostringstream ss(std::ios::binary);
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    SaveGameSnapshot(ar, *this);
  }
  std::string const& buf = ss.str();
  out.assign(buf.begin(), buf.end());
}

void Game::LoadSnapshot(std::vector<uint8_t> const& in) {
  std::string buf(in.begin(), in.end());
  std::istringstream ss(std::move(buf), std::ios::binary);
  cereal::PortableBinaryInputArchive ar(ss);
  LoadGameSnapshot(ar, *this);
}

void Game::SaveSnapshotFast(GameSnapshot& snap) const {
  snap.rand = rand;
  snap.cycles = cycles;
  snap.screen_flash = screen_flash;
  snap.last_killed_idx = last_killed_idx;
  snap.got_changed = got_changed;
  snap.holdazone = holdazone;

  for (std::size_t i = 0; i < worms.size() && i < snap.worms.size(); ++i) {
    SaveWormSimState(snap.worms[i], *worms[i]);
  }

  // ExactObjectList<T,N> contents are trivially copyable POD blocks; the
  // compiler-generated copy assignment is a straight memcpy of the fixed
  // arr/freeList/count layout.
  snap.bonuses = bonuses;
  snap.wobjects = wobjects;
  snap.sobjects = sobjects;
  snap.nobjects = nobjects;

  snap.bobjects_count = bobjects.count;
  if (snap.bobjects_arr.size() < bobjects.limit) {
    snap.bobjects_arr.resize(bobjects.limit);
  }
  if (bobjects.count > 0) {
    std::memcpy(snap.bobjects_arr.data(), bobjects.arr.data(), bobjects.count * sizeof(BObject));
  }

  std::size_t const kCells =
      static_cast<std::size_t>(level.width) * static_cast<std::size_t>(level.height);
  if (snap.level_data.size() != kCells) {
    snap.level_data.resize(kCells);
  }
  if (snap.level_materials.size() != kCells) {
    snap.level_materials.resize(kCells);
  }
  if (kCells > 0) {
    std::memcpy(snap.level_data.data(), level.material_id.data(), kCells);
    std::memcpy(snap.level_materials.data(), level.materials.data(), kCells * sizeof(Material));
  }
  // display_data is static (never modified during simulation) so only
  // display_valid is snapshotted; display_data is left untouched on restore.
  if (!level.display_valid.empty() && !snap.level_display_valid.empty()) {
    std::memcpy(snap.level_display_valid.data(), level.display_valid.data(), kCells);
  }
}

void Game::LoadSnapshotFast(GameSnapshot const& snap) {
  rand = snap.rand;
  cycles = snap.cycles;
  screen_flash = snap.screen_flash;
  last_killed_idx = snap.last_killed_idx;
  got_changed = snap.got_changed;
  holdazone = snap.holdazone;

  for (std::size_t i = 0; i < worms.size() && i < snap.worms.size(); ++i) {
    RestoreWormSimState(*worms[i], snap.worms[i]);
  }

  bonuses = snap.bonuses;
  wobjects = snap.wobjects;
  sobjects = snap.sobjects;
  nobjects = snap.nobjects;

  bobjects.count = snap.bobjects_count;
  if (snap.bobjects_count > 0) {
    std::memcpy(bobjects.arr.data(), snap.bobjects_arr.data(),
                snap.bobjects_count * sizeof(BObject));
  }

  std::size_t const kCells =
      static_cast<std::size_t>(level.width) * static_cast<std::size_t>(level.height);
  if (kCells > 0) {
    std::memcpy(level.material_id.data(), snap.level_data.data(), kCells);
    std::memcpy(level.materials.data(), snap.level_materials.data(), kCells * sizeof(Material));
  }
  // display_data is static; restore only display_valid.
  if (!snap.level_display_valid.empty() && !level.display_valid.empty()) {
    std::memcpy(level.display_valid.data(), snap.level_display_valid.data(), kCells);
  }
}
