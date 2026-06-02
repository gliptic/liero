#pragma once

#include <vector>
#include "gfx/bitmap.hpp"
#include "state.hpp"
#include "stats_recorder.hpp"

struct Game;

// Post-game statistics display.
struct StatsState : AppState {
  StatsState(NormalStatsRecorder& recorder, Game& game, bool is_multiplayer = false);

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;
  bool WantsMenuFlip() const override { return false; }

 private:
  NormalStatsRecorder& recorder_;
  Game& game_;
  bool isMultiplayer_;

  Bitmap bg_;
  double offset_ = 0, destOffset_ = 0;
  double pane_ = 0, destPane_ = 0;

  std::vector<WeaponStats> combinedWeaponStats_;
  std::vector<WeaponStats> weaponStats_[2];
  std::vector<double> wormDamages_[2];
  std::vector<double> wormTotalHpDiff_;
};
