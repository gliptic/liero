#include "constants.hpp"
#include "game.hpp"
#include "gfx/color.hpp"
#include "math.hpp"
#include "worm.hpp"

void Ninjarope::Process(Worm& owner, Game& game) {
  Common& common = *game.common;

  if (out) {
    pos += vel;

    auto ipos = Ftoi(pos);

    anchor = 0;
    for (std::size_t i = 0; i < game.worms.size(); ++i) {
      Worm& w = *game.worms[i];

      if (&w != &owner && CheckForSpecWormHit(game, ipos.x, ipos.y, 1, w)) {
        anchor = &w;
        break;
      }
    }

    fixedvec diff = pos - owner.pos;

    fixedvec force((diff.x << LC(NRForceShlX)) / LC(NRForceDivX),
                   (diff.y << LC(NRForceShlY)) / LC(NRForceDivY));

    cur_len = (VectorLength(Ftoi(diff.x), Ftoi(diff.y)) + 1) << LC(NRForceLenShl);

    if (ipos.x <= 0 || ipos.x >= game.level.width - 1 || ipos.y <= 0 ||
        ipos.y >= game.level.height - 1 || game.level.Mat(ipos).DirtRock()) {
      if (!attached) {
        length = LC(NRAttachLength);
        attached = true;

        if (game.level.Inside(ipos)) {
          if (game.level.Mat(ipos).AnyDirt()) {
            PalIdx pix = game.level.Pixel(ipos);
            for (int i = 0; i < 11; ++i)  // TODO: Check 11 and read from exe
            {
              common.nobject_types[2].Create2(game, game.rand(128), fixedvec(), pos, pix,
                                              owner.index, 0);
            }
          }
        }
      }

      vel.Zero();
    } else if (anchor) {
      if (!attached) {
        length =
            LC(NRAttachLength);  // TODO: Should this value be separate from the non-worm attaching?
        attached = true;
      }

      if (cur_len > length) {
        anchor->vel -= force / cur_len;
      }

      vel = anchor->vel;
      pos = anchor->pos;
    } else {
      attached = false;
    }

    if (attached) {
      // curLen can't be 0

      if (cur_len > length) {
        owner.vel += force / cur_len;
      }
    } else {
      vel.y += LC(NinjaropeGravity);

      if (cur_len > length) {
        vel -= force / cur_len;
      }
    }
  }
}
