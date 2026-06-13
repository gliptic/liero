#include "stats_recorder.hpp"

#include <chrono>
#include "common.hpp"
#include "game.hpp"
#include "text.hpp"

void StatsRecorder::DamagePotential(Worm* by_worm, WormWeapon* weapon, int hp) {}

void StatsRecorder::DamageDealt(Worm* by_worm, WormWeapon* weapon, Worm* to_worm, int hp,
                                bool has_hit) {}

void StatsRecorder::Shot(Worm* by_worm, WormWeapon* weapon) {}

void StatsRecorder::Hit(Worm* by_worm, WormWeapon* weapon, Worm* to_worm) {}

void StatsRecorder::AfterSpawn(Worm* worm) {}

void StatsRecorder::AfterDeath(Worm* worm) {}

void StatsRecorder::Finish(Game& game) {}

void StatsRecorder::PreTick(Game& game) {}

void StatsRecorder::Tick(Game& game) {}

void StatsRecorder::AiProcessTime(Worm* worm, std::chrono::nanoseconds time) {}

void StatsRecorder::Reset(int /*lev_w*/, int /*lev_h*/) {}

void NormalStatsRecorder::DamagePotential(Worm* by_worm, WormWeapon* weapon, int hp) {
  if (speculative) {
    return;
  }
  if (!by_worm || !weapon) {
    return;
  }

  WormStats& ws = worms[by_worm->index];
  WeaponStats& weap = ws.weapons[weapon->type->id];
  weap.potential_hp += hp;
}

void NormalStatsRecorder::DamageDealt(Worm* by_worm, WormWeapon* weapon, Worm* to_worm, int hp,
                                      bool has_hit) {
  if (speculative) {
    return;
  }
  assert(to_worm);

  auto& w = worms[to_worm->index];
  w.damage += hp;
  w.worm_frame_stats.back().damage += hp;
  w.damage_hm.IncArea(Ftoi(to_worm->pos.x), Ftoi(to_worm->pos.y), hp);

  if (by_worm) {
    if (by_worm != to_worm) {
      worms[by_worm->index].damage_dealt += hp;
    } else {
      worms[by_worm->index].self_damage += hp;
    }
  }

  if (!by_worm || !weapon) {
    return;
  }

  if (by_worm != to_worm)  // Don't count if projectile already hit
  {
    WormStats& ws = worms[by_worm->index];
    WeaponStats& weap = ws.weapons[weapon->type->id];
    if (!has_hit) {
      weap.actual_hp += hp;
    }
    weap.total_hp += hp;
  }
}

void NormalStatsRecorder::Shot(Worm* by_worm, WormWeapon* weapon) {
  if (speculative) {
    return;
  }
  if (!by_worm || !weapon) {
    return;
  }

  WormStats& ws = worms[by_worm->index];
  WeaponStats& weap = ws.weapons[weapon->type->id];
  weap.potential_hits += 1;
}

void NormalStatsRecorder::Hit(Worm* by_worm, WormWeapon* weapon, Worm* to_worm) {
  if (speculative) {
    return;
  }
  assert(to_worm);

  if (!by_worm || !weapon) {
    return;
  }

  if (by_worm != to_worm) {
    WormStats& ws = worms[by_worm->index];
    WeaponStats& weap = ws.weapons[weapon->type->id];
    weap.actual_hits += 1;
  }
}

void NormalStatsRecorder::AfterSpawn(Worm* worm) {
  if (speculative) {
    return;
  }
  WormStats& w = worms[worm->index];
  w.spawn_time = frame;
}

void NormalStatsRecorder::AfterDeath(Worm* worm) {
  if (speculative) {
    return;
  }
  WormStats& w = worms[worm->index];
  w.life_spans.emplace_back(w.spawn_time, frame);
  w.spawn_time = -1;
}

void NormalStatsRecorder::PreTick(Game& game) {
  if (speculative) {
    return;
  }
  frame_start = std::chrono::steady_clock::now();

  for (auto& w : worms) {
    w.worm_frame_stats.emplace_back();

    Worm const& worm = *game.worms[w.index];

    int h = std::max(worm.health, 0);
    if (!worm.visible) {
      h = worm.settings->health;
    }

    w.worm_frame_stats.back().total_hp = worm.lives * worm.settings->health + h;
  }
}

void NormalStatsRecorder::Tick(Game& game) {
  if (speculative) {
    return;
  }
  auto frame_end = std::chrono::steady_clock::now();
  process_time_total +=
      std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();

  for (auto const& w : game.worms) {
    auto& ws = worms[w->index];
    if (w->visible) {
      presence.Inc(Ftoi(w->pos.x), Ftoi(w->pos.y));
      ws.presence.Inc(Ftoi(w->pos.x), Ftoi(w->pos.y));

      bool ok = true;
      if (!w->control_states[Worm::Control::kFire] &&
          (!w->control_states[Worm::Control::kChange] ||
           (!w->control_states[Worm::Control::kLeft] &&
            !w->control_states[Worm::Control::kRight])) &&
          w->weapons[w->current_weapon].loading_left == 0 &&
          std::find_if(w->weapons, w->weapons + 5,
                       [](WormWeapon& ww) { return ww.loading_left > 0; }) != w->weapons + 5) {
        ok = false;
      }

      ws.weapon_change_good += ok;
      ws.weapon_change_bad += !ok;
    }
  }

  ++frame;
}

void NormalStatsRecorder::Finish(Game& game) {
  for (int i = 0; i < 2; ++i) {
    auto const& gw = game.worms[i];
    WormStats& w = worms[i];
    if (w.spawn_time >= 0) {
      w.life_spans.emplace_back(w.spawn_time, frame);
      w.spawn_time = -1;
    }
    w.lives = gw->lives;
    w.timer = gw->timer;
    w.kills = gw->kills;
  }

  game_time = frame;
}

void NormalStatsRecorder::AiProcessTime(Worm* worm, std::chrono::nanoseconds time) {
  if (speculative) {
    return;
  }
  WormStats& w = worms[worm->index];
  w.ai_process_time += time;
}

void NormalStatsRecorder::Reset(int lev_w, int lev_h) {
  presence = Heatmap(lev_w / 2, lev_h / 2, lev_w, lev_h);
  for (auto& w : worms) {
    w.Reset(lev_w, lev_h);
  }
}
