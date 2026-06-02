#pragma once

#include <chrono>
#include "gfx/blit.hpp"
#include "worm.hpp"

struct Common;
struct Renderer;

struct StatsRecorder {
  virtual ~StatsRecorder() = default;

  virtual void DamagePotential(Worm* by_worm, WormWeapon* weapon, int hp);
  virtual void DamageDealt(Worm* by_worm, WormWeapon* weapon, Worm* to_worm, int hp, bool has_hit);

  virtual void Shot(Worm* by_worm, WormWeapon* weapon);
  virtual void Hit(Worm* by_worm, WormWeapon* weapon, Worm* to_worm);

  virtual void AfterSpawn(Worm* worm);
  virtual void AfterDeath(Worm* worm);
  virtual void PreTick(Game& game);
  virtual void Tick(Game& game);
  virtual void Finish(Game& game);

  virtual void AiProcessTime(Worm* worm, std::chrono::nanoseconds time);

  // When true, all recording is suppressed. Set during predicted /
  // resim frames to avoid double-counting.
  bool speculative = false;
};

struct WeaponStats {
  WeaponStats() : potential_hp(0), actual_hp(0), potential_hits(0), actual_hits(0), total_hp(0) {}

  void Combine(WeaponStats const& other) {
    potential_hits += other.potential_hits;
    potential_hp += other.potential_hp;
    actual_hits += other.actual_hits;
    actual_hp += other.actual_hp;
    total_hp += other.total_hp;
  }

  int potential_hp, actual_hp;
  int potential_hits, actual_hits;
  int total_hp;
  int index;
};

struct WormFrameStats {
  WormFrameStats() : damage(0), total_hp(0) {}

  int damage;
  int total_hp;
};

struct WormStats {
  WormStats()
      : damage(0),
        damage_dealt(0),
        self_damage(0),
        damage_hm(504 / 2, 350 / 2, 504, 350),
        presence(504 / 2, 350 / 2, 504, 350),
        weapon_change_good(0),
        weapon_change_bad(0),
        spawn_time(-1),
        lives(0),
        timer(0),
        kills(0),
        ai_process_time(0) {
    for (int i = 0; i < 40; ++i) weapons[i].index = i;
  }

  std::vector<std::pair<int, int> > life_spans;

  void LifeStats(int& min, int& max) {
    min = 0;
    max = 0;
    if (life_spans.empty()) return;

    min = life_spans[0].second - life_spans[0].first;
    max = min;

    for (size_t i = 1; i < life_spans.size(); ++i) {
      int len = life_spans[i].second - life_spans[i].first;
      max = std::max(len, max);
      min = std::min(len, min);
    }
  }

  WeaponStats weapons[40];
  int damage, damage_dealt, self_damage;
  Heatmap damage_hm, presence;
  int weapon_change_good, weapon_change_bad;

  std::vector<WormFrameStats> worm_frame_stats;

  int spawn_time;
  int index;

  int lives, timer, kills;
  std::chrono::nanoseconds ai_process_time;
};

struct NormalStatsRecorder : StatsRecorder {
  NormalStatsRecorder()
      : frame(0),
        frame_start(std::chrono::steady_clock::now()),
        process_time_total(0),
        game_time(0),
        presence(504 / 2, 350 / 2, 504, 350) {
    for (int i = 0; i < 2; ++i) {
      worms[i].index = i;
    }
  }

  int frame;
  WormStats worms[2];
  std::chrono::time_point<std::chrono::steady_clock> frame_start;
  int64_t process_time_total;
  int game_time;

  Heatmap presence;

  void DamagePotential(Worm* by_worm, WormWeapon* weapon, int hp);
  void DamageDealt(Worm* by_worm, WormWeapon* weapon, Worm* to_worm, int hp, bool has_hit);

  void Shot(Worm* by_worm, WormWeapon* weapon);
  void Hit(Worm* by_worm, WormWeapon* weapon, Worm* to_worm);

  void AfterSpawn(Worm* worm);
  void AfterDeath(Worm* worm);
  void PreTick(Game& game);
  void Tick(Game& game);

  void Finish(Game& game);
  void AiProcessTime(Worm* worm, std::chrono::nanoseconds time);
};
