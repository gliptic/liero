#include "bonus.hpp"
#include <cstdlib>
#include "constants.hpp"
#include "game.hpp"

void Bonus::Process(Game& game) {
  Common& common = *game.common;

  y += vel_y;

  int const kIx = Ftoi(x);
  int const kIy = Ftoi(y);

  assert(kIx >= 0 && kIx < game.level.width);

  if (game.level.Inside(kIx, kIy + 1) && game.level.Mat(kIx, kIy + 1).Background()) {
    vel_y += LC(BonusGravity);
  }

  int const kInewY = Ftoi(y + vel_y);
  if (kInewY < 0 || kInewY >= game.level.height - 1 || game.level.Mat(kIx, kInewY).DirtRock()) {
    vel_y = -(vel_y * LC(BonusBounceMul)) / LC(BonusBounceDiv);

    if (std::abs(vel_y) < 100)  // TODO: Read from EXE
      vel_y = 0;
  }

  if (--timer <= 0) {
    common.sobject_types[common.bonus_s_objects[frame]].Create(game, kIx, kIy, 0, nullptr);
    if (used) game.bonuses.Free(this);
  }
}
