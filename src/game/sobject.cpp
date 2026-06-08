#include "sobject.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include "console.hpp"
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "mixer/player.hpp"
#include "text.hpp"
#include "viewport.hpp"
#include "worm.hpp"

// NOLINTNEXTLINE(misc-no-recursion) — sobject effects can spawn child sobjects; recursion mirrors the data flow and is bounded by the spec.
void SObjectType::Create(Game& game, int x, int y, int owner_idx, WormWeapon* fired_by,
                         WObject* from) const {
  Common& common = *game.common;
  SObject& obj = *game.sobjects.NewObjectReuse();

  assert(num_sounds < 10);

  if (start_sound >= 0) {
    game.sound_player->Play(game.rand(num_sounds) + start_sound);
  }

  for (auto& viewport : game.viewports) {
    Viewport& v = *viewport;

    if (x > v.x && x < v.x + v.rect.Width() && y > v.y && y < v.y + v.rect.Height()) {
      v.shake = std::max(Itof(shake), v.shake);
    }
  }

  obj.id = id;
  obj.x = x - 8;
  obj.y = y - 8;
  obj.cur_frame = 0;
  obj.anim_delay = anim_delay;

  game.screen_flash = std::max(flash, game.screen_flash);

  Worm* owner = game.WormByIdx(owner_idx);

  game.stats_recorder->DamagePotential(owner, fired_by, damage);

  if (damage > 0) {
    for (std::size_t i = 0; i < game.worms.size(); ++i) {
      Worm& w = *game.worms[i];

      int const kWix = Ftoi(w.pos.x);
      int const kWiy = Ftoi(w.pos.y);

      if (kWix < x + detect_range && kWix > x - detect_range && kWiy < y + detect_range &&
          kWiy > y - detect_range) {
        int delta = kWix - x;
        int power = detect_range - std::abs(delta);
        int power_sum = power;

        if (std::abs(w.vel.x) < Itof(2))  // TODO: Read from EXE
        {
          if (delta > 0) {
            w.vel.x += blow_away * power;
          } else {
            w.vel.x -= blow_away * power;
          }
        }

        delta = kWiy - y;
        power = detect_range - std::abs(delta);
        power_sum = (power_sum + power) / 2;

        if (std::abs(w.vel.y) < Itof(2))  // TODO: Read from EXE
        {
          if (delta > 0) {
            w.vel.y += blow_away * power;
          } else {
            w.vel.y -= blow_away * power;
          }
        }

        int z = damage * power_sum;
        if (detect_range) {
          z /= detect_range;
        }

        if (from && !from->has_hit) {
          game.stats_recorder->Hit(owner, fired_by, &w);
          from->has_hit = true;
        }

        if (w.health > 0) {
          game.DoDamage(w, z, owner_idx);
          game.stats_recorder->DamageDealt(owner, fired_by, &w, z, /*has_hit=*/false);

          int const kBloodAmount = game.settings->blood * power_sum / 100;

          if (kBloodAmount > 0) {
            for (int i = 0; i < kBloodAmount; ++i) {
              int const kAngle = game.rand(128);
              common.nobject_types[6].Create2(game, kAngle, w.vel / 3, w.pos, 0, w.index, fired_by);
            }
          }

          if (game.rand(3) == 0) {
            int const kSnd =
                18 + game.rand(3);  // NOTE: MUST be outside the unpredictable branch below
            if (!game.sound_player->IsPlaying(&w)) {
              game.sound_player->Play(kSnd, &w);
            }
          }
        }
      }
    }  // for( ... worms ...

    int const kObjBlowAway = blow_away / 3;  // TODO: Read from EXE

    auto wr = game.wobjects.All();
    for (WObject* i = nullptr; (i = wr.Next());) {
      Weapon const& weapon = *i->type;

      if (weapon.affect_by_explosions) {
        auto ipos = Ftoi(i->pos);
        if (ipos.x < x + detect_range && ipos.x > x - detect_range && ipos.y < y + detect_range &&
            ipos.y > y - detect_range) {
          int delta = ipos.x - x;
          int power = detect_range - std::abs(delta);

          if (power > 0) {
            if (delta > 0) {
              i->vel.x += kObjBlowAway * power;
            } else if (delta < 0) {
              i->vel.x -= kObjBlowAway * power;
            }
          }

          delta = ipos.y - y;
          power = detect_range - std::abs(delta);

          if (power > 0) {
            if (delta > 0) {
              i->vel.y += kObjBlowAway * power;
            } else if (delta < 0) {
              i->vel.y -= kObjBlowAway * power;
            }
          }

          if (weapon.chain_explosion) {
            i->BlowUpObject(game, owner_idx);
          }
        }
      }  // if( ... affectByExplosions ...
    }  // for( ... wobjects ...

    auto nr = game.nobjects.All();
    for (NObject* i = nullptr; (i = nr.Next());) {
      NObjectType const& t = *i->type;

      if (t.affect_by_explosions) {
        auto ipos = Ftoi(i->pos);
        if (ipos.x < x + detect_range && ipos.x > x - detect_range && ipos.y < y + detect_range &&
            ipos.y > y - detect_range) {
          int delta = ipos.x - x;
          int power = detect_range - std::abs(delta);

          if (power > 0) {
            if (delta > 0) {
              i->vel.x += kObjBlowAway * power;
            } else if (delta < 0) {
              i->vel.x -= kObjBlowAway * power;
            }
          }

          delta = ipos.y - y;
          power = detect_range - std::abs(delta);

          if (power > 0) {
            if (delta > 0) {
              i->vel.y += kObjBlowAway * power;
            } else if (delta < 0) {
              i->vel.y -= kObjBlowAway * power;
            }
          }
        }
      }
    }

    {
      int const kWidth = detect_range / 2;

      Rect rect(x - kWidth, y - kWidth, x + kWidth + 1, y + kWidth + 1);

      rect.Intersect(game.level.Bounds());

      for (int y = rect.y1; y < rect.y2; ++y) {
        for (int x = rect.x1; x < rect.x2; ++x) {
          if (game.level.Mat(x, y).AnyDirt() && game.rand(8) == 0) {
            PalIdx const kPix = game.level.Pixel(x, y);
            int const kAngle = game.rand(128);
            common.nobject_types[2].Create2(game, kAngle, fixedvec(), Itof(IVec2(x, y)), kPix,
                                            owner_idx, fired_by);
          }
        }
      }
    }

  }  // if(damage ...

  if (dirt_effect >= 0) {
    DrawDirtEffect(common, game.rand, game.level, dirt_effect, x - 7, y - 7);

    if (game.settings->shadow) {
      CorrectShadow(common, game.level, Rect(x - 10, y - 10, x + 11, y + 11));
    }
  }

  auto br = game.bonuses.All();
  for (Bonus const* i = nullptr; (i = br.Next());) {
    int const kIx = Ftoi(i->x);
    int const kIy = Ftoi(i->y);

    if (kIx > x - detect_range && kIx < x + detect_range && kIy > y - detect_range &&
        kIy < y + detect_range) {
      game.bonuses.Free(br);
      common.sobject_types[0].Create(game, kIx, kIy, owner_idx, fired_by);
    }
  }  // for( ... bonuses ...
}

void SObject::Process(Game& game) {
  Common& common = *game.common;
  SObjectType const& t = common.sobject_types[id];

  if (--anim_delay <= 0) {
    anim_delay = t.anim_delay;
    ++cur_frame;
    if (cur_frame > t.num_frames) {
      game.sobjects.Free(this);
    }
  }
}
