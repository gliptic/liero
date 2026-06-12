#include "statsState.hpp"

#include <algorithm>
#include <chrono>
#include <type_traits>
#include "game.hpp"
#include "gfx.hpp"
#include "gfx/text_cell.hpp"
#include "net/session.hpp"
#include "rematchState.hpp"
#include "stats.hpp"
#include "text.hpp"

using cell = TextCell;
using std::vector;

static std::string Percent(int nom, int den) {
  if (den == 0) {
    return "";
  }

  const int kBufMax = 256;
  char buf[kBufMax];
  // NOLINTNEXTLINE(cert-err33-c) — buffer is 256 bytes for a "%.2f%%" formatted number; fits trivially.
  std::snprintf(buf, kBufMax * sizeof(char), "%.2f%%", static_cast<double>(nom) * 100.0 / den);
  return buf;
}

struct StatsRenderer {
  static int const kTextColor = 7;

  StatsRenderer(Renderer& renderer, Game& game, NormalStatsRecorder& stats, Common& common)
      : renderer(renderer),
        game(game),
        stats(stats),
        common(common),
        pane_width(renderer.render_res_x - 20) {}

  static int const kPaneX = 10;

  template <typename P>
  void Pane(int n, int left_x, int top_y, P const& p) {
    offs_x = n * renderer.render_res_x + left_x;

    if (offs_x >= -renderer.render_res_x && offs_x < renderer.render_res_x) {
      y = top_y;
      y += 10;

      DrawRoundedBox(renderer.bmp, offs_x + kPaneX, y, 0, 2000, pane_width);

      y += 10;

      p();
    }
  }

  template <typename B>
  bool Hblock(int height, B const& b) {
    bool ran = false;
    if (y < renderer.render_res_y && y + height > 0) {
      b();
      ran = true;
    }
    y += height;
    return ran;
  }

  void DrawWorms() {
    Hblock(20, [this] {
      for (int i = 0; i < 2; ++i) {
        int const kX =
            renderer.render_res_x / 2 + (i == 0 ? -1 : 1) * (renderer.render_res_x / 4) + offs_x;
        BlitImage(renderer.bmp, common.WormSpriteObj(2, i == 0 ? 1 : 0, i), kX - 8, y);

        cell c(i == 0 ? TextCell::kRight : TextCell::kLeft);
        common.font.DrawString(renderer.bmp, c << game.worms[i]->settings->name,
                               kX + (i == 0 ? -16 : 16), y + 2, kTextColor);
      }
    });
  }

  void DrawWorm(int i) {
    bool const kVisible = Hblock(20, [this, i] {
      int const kX = renderer.render_res_x / 2 + offs_x;
      BlitImage(renderer.bmp, common.WormSpriteObj(2, i == 0 ? 1 : 0, i), kX - 8, y);

      cell c(i == 0 ? TextCell::kRight : TextCell::kLeft);
      common.font.DrawString(renderer.bmp, c << game.worms[i]->settings->name,
                             kX + (i == 0 ? -16 : 16), y + 2, kTextColor);
    });

    if (!kVisible) {
      int const kX = 18 + offs_x;
      BlitImage(renderer.bmp, common.WormSpriteObj(2, i == 0 ? 1 : 0, i), kX - 8, 10);
    }
  }

  template <typename Ws>
  void DrawWormStat(char const* name, Ws worm_stat) {
    Hblock(11, [this, name, &worm_stat] {
      common.font.DrawString(renderer.bmp, cell(TextCell::kCenter).Ref() << name,
                             renderer.render_res_x / 2 + offs_x, y, kTextColor);

      for (int i = 0; i < 2; ++i) {
        TextCell::Placement const kP = i == 0 ? TextCell::kRight : TextCell::kLeft;
        int const kX = renderer.render_res_x / 2 + (i == 0 ? -40 : 40) + offs_x;

        WormStats& w = stats.worms[i];
        cell c(kP);
        worm_stat(w, c);
        common.font.DrawString(renderer.bmp, c, kX, y, kTextColor);
      }
    });
  }

  template <typename Stat>
  void DrawStat(char const* name, Stat stat) {
    Hblock(11, [this, name, &stat] {
      common.font.DrawString(renderer.bmp, cell(TextCell::kRight).Ref() << name,
                             renderer.render_res_x / 2 + offs_x, y, kTextColor);

      int const kX = renderer.render_res_x / 2 + 10 + offs_x;

      cell c(TextCell::kLeft);
      stat(c);
      common.font.DrawString(renderer.bmp, c, kX, y, kTextColor);
    });
  }

  void DrawWormStat(char const* name, int(WormStats::* field)) {
    DrawWormStat(name, [field](WormStats& w, cell& c) { c << w.*field; });
  }

  void Section(cell const& c, int level = 1) {
    int x = offs_x + 20;
    int color = kTextColor;
    if (level > 0) {
      x += 20;
      color = 3;
    }

    common.font.DrawString(renderer.bmp, c, x, y, color);

    if (level == 0) {
      y += 11;
    }
  }

  void Gap(int n = 5) { y += n; }

  void WeaponStats(vector<WeaponStats> const& list) {
    for (const auto& ws : list) {
      if (ws.total_hp > 0) {
        Section(cell().Ref() << common.weapons[ws.index].name);
        DrawStat("hits", [ws](cell& c) {
          c << ws.actual_hits << "/" << ws.potential_hits << " ("
            << Percent(ws.actual_hits, ws.potential_hits) << ")";
        });
        if (ws.potential_hp > 0) {
          DrawStat("damage", [ws](cell& c) {
            c << ws.actual_hp << "/" << ws.potential_hp << " ("
              << Percent(ws.actual_hp, ws.potential_hp) << ")";
          });
        }
        if (ws.total_hp != ws.actual_hp) {
          DrawStat("total damage", [ws](cell& c) { c << ws.total_hp; });
        }
        Gap();
      }
    }
  }

  void Graph(vector<double> const& data, int height, int color, int neg_color, bool balanced) {
    y += 2;
    Hblock(height, [&, this] {
      int const kStart = 20 + offs_x;
      DrawGraph(renderer.bmp, data, height, kStart, y, color, neg_color, balanced);
    });
    y += 7;
  }

  void Heatmap(Heatmap& hm) {
    y += 2;
    Hblock(hm.height, [&] {
      int const kStartX = kPaneX + pane_width / 2 - (hm.width / 2) + offs_x;
      int const kStartY = y;

      DrawHeatmap(renderer.bmp, kStartX, kStartY, hm);
    });
    y += 7;
  }

  Renderer& renderer;
  Game& game;
  NormalStatsRecorder& stats;
  Common& common;
  int pane_width = 300;
  int offs_x, y;
};

static void SortWeaponStats(vector<WeaponStats>& ws) {
  std::ranges::sort(
      ws, [](WeaponStats const& a, WeaponStats const& b) { return a.actual_hp > b.actual_hp; });
}

StatsState::StatsState(NormalStatsRecorder& recorder, Game& game, bool is_multiplayer)
    : recorder_(recorder), game_(game), isMultiplayer_(is_multiplayer) {}

void StatsState::Enter() {
  gfx->ClearKeys();

  Common const& common = *game_.common;

  int const kGraphWidth = gfx->play_renderer.render_res_x - 20 - 20;

  for (int i = 0; i < 2; ++i) {
    wormDamages_[i] = Stretch(
        Convert<double>(Pluck(recorder_.worms[i].worm_frame_stats, &WormFrameStats::damage)),
        kGraphWidth);
    Cumulative(wormDamages_[i]);
    Normalize(wormDamages_[i], 50);
  }

  vector<double> worm_total_hp[2];
  for (int i = 0; i < 2; ++i) {
    worm_total_hp[i] =
        Convert<double>(Pluck(recorder_.worms[i].worm_frame_stats, &WormFrameStats::total_hp));
  }

  wormTotalHpDiff_ = Stretch(Zip(worm_total_hp[0], worm_total_hp[1], std::minus<>()), kGraphWidth);
  Normalize(wormTotalHpDiff_, 100);

  for (int i = 0; i < 40; ++i) {
    auto ws = recorder_.worms[0].weapons[i];
    ws.Combine(recorder_.worms[1].weapons[i]);
    combinedWeaponStats_.push_back(ws);

    weaponStats_[0].push_back(recorder_.worms[0].weapons[i]);
    weaponStats_[1].push_back(recorder_.worms[1].weapons[i]);
  }

  SortWeaponStats(combinedWeaponStats_);
  SortWeaponStats(weaponStats_[0]);
  SortWeaponStats(weaponStats_[1]);

  bg_.Copy(gfx->play_renderer.bmp);
}

void StatsState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool StatsState::Update() {
  // Keep the network session alive while viewing stats
  if (isMultiplayer_ && gfx->net_session) {
    gfx->net_session->Update();
    auto state = gfx->net_session->State();
    if (state == NetSession::kDisconnected || state == NetSession::kFailed) {
      gfx->net_session.reset();
      isMultiplayer_ = false;
    }
  }

  if (gfx->TestSdlKey(SDL_SCANCODE_DOWN) || gfx->TestControl(WormSettingsExtensions::kDown) ||
      gfx->TestGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
    destOffset_ -= 10;
  } else if (gfx->TestSdlKey(SDL_SCANCODE_UP) || gfx->TestControl(WormSettingsExtensions::kUp) ||
             gfx->TestGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
    destOffset_ = std::min(destOffset_ + 10.0, 0.0);
  } else if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RIGHT) ||
             gfx->TestControlOnce(WormSettingsExtensions::kRight) ||
             gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
    destPane_ = std::min(destPane_ + 1.0, 1.0);
  } else if (gfx->TestSdlKeyOnce(SDL_SCANCODE_LEFT) ||
             gfx->TestControlOnce(WormSettingsExtensions::kLeft) ||
             gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
    destPane_ = std::max(destPane_ - 1.0, -1.0);
  } else if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RETURN) || gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) ||
             gfx->TestControlOnce(WormSettingsExtensions::kFire) ||
             gfx->TestControlOnce(WormSettingsExtensions::kJump) ||
             gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH) ||
             gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST)) {
    Fill(gfx->play_renderer.bmp, 0);
    gfx->ClearKeys();

    if (isMultiplayer_ && gfx->net_session) {
      gfx->state_stack.ScheduleReplaceTop(std::make_unique<RematchState>(game_));
      return true;
    }

    return false;  // pop
  }

  pane_ = pane_ * 0.89 + destPane_ * 0.11;
  offset_ = offset_ * 0.89 + destOffset_ * 0.11;

  offset_ = std::min<double>(offset_, 0);

  return true;
}

void StatsState::Draw() {
  Common& common = *game_.common;

  // The stats screen renders on the plain EXE palette (no rotation/fade);
  // finalize it before drawing so blits resolve through fresh pal32.
  gfx->play_renderer.pal = common.exepal;
  gfx->play_renderer.UpdatePal32();

  gfx->play_renderer.bmp.Copy(bg_);

  StatsRenderer renderer(gfx->play_renderer, game_, recorder_, common);

  int const kOffsX = static_cast<int>(std::floor(pane_ * -renderer.renderer.render_res_x));
  int const kOffsY = static_cast<int>(offset_);

  renderer.Pane(0, kOffsX, kOffsY, [&] {
    renderer.DrawWorms();

    int const kOldy = renderer.y;
    renderer.y -= 20;

    renderer.DrawStat(common.texts.game_modes[game_.settings->game_mode].c_str(),
                      [&](cell& c) { c << TimeToStringFrames(recorder_.game_time); });

    renderer.y = kOldy;

    renderer.DrawWormStat("ai processing", [&](WormStats& w, cell& c) {
      c << static_cast<int>(
               std::chrono::duration_cast<std::chrono::milliseconds>(w.ai_process_time).count())
        << "ms";
    });

    if (game_.settings->game_mode == Settings::kGmHoldazone ||
        game_.settings->game_mode == Settings::kGmGameOfTag) {
      renderer.DrawWormStat("timer", [&](WormStats& w, cell& c) { c << TimeToString(w.timer); });
    } else {
      renderer.DrawWormStat("lives left", &WormStats::lives);
    }
    renderer.DrawWormStat("kills", &WormStats::kills);
    renderer.DrawWormStat("damage dealt", &WormStats::damage_dealt);
    renderer.DrawWormStat("damage received", &WormStats::damage);
    renderer.DrawWormStat("damage to self", &WormStats::self_damage);

    renderer.DrawWormStat("shortest life", [](WormStats& w, cell& c) {
      int min = 0;
      int max = 0;
      w.LifeStats(min, max);
      c << TimeToStringFrames(min);
    });

    renderer.DrawWormStat("longest life", [](WormStats& w, cell& c) {
      int min = 0;
      int max = 0;
      w.LifeStats(min, max);
      c << TimeToStringFrames(max);
    });

    renderer.DrawWormStat("loading efficiency", [](WormStats& w, cell& c) {
      c << Percent(w.weapon_change_good, w.weapon_change_good + w.weapon_change_bad);
    });

    renderer.Gap();

    renderer.WeaponStats(combinedWeaponStats_);

    renderer.Section(cell().Ref() << "Total health difference", 0);

    renderer.Graph(wormTotalHpDiff_, 100, Palette::kWormColorBlocks[0].colour_index,
                   Palette::kWormColorBlocks[1].colour_index, /*balanced=*/true);

    renderer.Section(cell().Ref() << "Presence", 0);
    renderer.Heatmap(recorder_.presence);
  });

  for (int i = 0; i < 2; ++i) {
    renderer.Pane(i == 0 ? -1 : 1, kOffsX, kOffsY, [&] {
      renderer.DrawWorm(i);

      renderer.WeaponStats(weaponStats_[i]);

      renderer.Section(cell().Ref() << "Damage over time", 0);
      renderer.Graph(wormDamages_[i], 50, Palette::kWormColorBlocks[i].colour_index, 0,
                     /*balanced=*/false);

      renderer.Section(cell().Ref() << "Presence", 0);
      renderer.Heatmap(recorder_.worms[i].presence);
      renderer.Section(cell().Ref() << "Damage", 0);
      renderer.Heatmap(recorder_.worms[i].damage_hm);
    });
  }
}
