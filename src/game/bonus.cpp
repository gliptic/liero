#include "bonus.hpp"
#include <cstdlib>
#include "constants.hpp"
#include "game.hpp"

void Bonus::Process(Game& game) {
  Common& common = *game.common;

  y += vel_y;

  int ix = Ftoi(x), iy = Ftoi(y);

  assert(ix >= 0 && ix < game.level.width);

  if (game.level.Inside(ix, iy + 1) && game.level.Mat(ix, iy + 1).Background()) {
    vel_y += LC(BonusGravity);
  }

  int inew_y = Ftoi(y + vel_y);
  if (inew_y < 0 || inew_y >= game.level.height - 1 || game.level.Mat(ix, inew_y).DirtRock()) {
    vel_y = -(vel_y * LC(BonusBounceMul)) / LC(BonusBounceDiv);

    if (std::abs(vel_y) < 100)  // TODO: Read from EXE
      vel_y = 0;
  }

  if (--timer <= 0) {
    common.sobject_types[common.bonus_s_objects[frame]].Create(game, ix, iy, 0, 0);
    if (used) game.bonuses.Free(this);
  }
}
