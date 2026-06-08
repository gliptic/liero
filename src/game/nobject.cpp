#include "nobject.hpp"
#include "bobject.hpp"
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "mixer/player.hpp"

NObject& NObjectType::Create(Game& game, fixedvec vel, fixedvec pos, int color, int owner_idx,
                             WormWeapon* fired_by) {
  NObject& obj = *game.nobjects.NewObjectReuse();

  obj.type = this;
  obj.owner_idx = owner_idx;
  obj.pos = pos;

  obj.vel = vel;

  // STATS
  obj.fired_by = fired_by;
  obj.has_hit = false;

  Worm* owner = game.WormByIdx(owner_idx);
  game.stats_recorder->DamagePotential(owner, fired_by, hit_damage);

  if (start_frame > 0) {
    obj.cur_frame = game.rand(num_frames + 1);
  } else if (color != 0) {
    obj.cur_frame = color;
  } else {
    obj.cur_frame = color_bullets;
  }

  obj.time_left = time_to_explo;

  if (time_to_explo_v) {
    obj.time_left -= game.rand(time_to_explo_v);
  }

  return obj;
}

NObject& NObjectType::Create1(Game& game, fixedvec vel, fixedvec pos, int color, int owner_idx,
                              WormWeapon* fired_by) {
  if (distribution) {
    vel.x += distribution - game.rand(distribution * 2);
    vel.y += distribution - game.rand(distribution * 2);
  }

  return Create(game, vel, pos, color, owner_idx, fired_by);
}

void NObjectType::Create2(Game& game, int angle, fixedvec vel, fixedvec pos, int color,
                          int owner_idx, WormWeapon* fired_by) {
  int const kRealSpeed = speed - game.rand(speed_v);

  vel += cossin_table[angle] * kRealSpeed / 100;

  // TODO: !REPLAYS Make the distributions use the same code
  if (distribution) {
    vel.x += game.rand(distribution * 2) - distribution;
    vel.y += game.rand(distribution * 2) - distribution;
  }

  auto& obj = Create(game, vel, pos, color, owner_idx, fired_by);

  obj.pos += obj.vel;
}

void NObject::Process(Game& game) {
  Common& common = *game.common;

  bool bounced = false;
  bool do_explode = false;

  pos += vel;

  auto inew_pos = Ftoi(pos + vel);
  auto ipos = Ftoi(pos);

  NObjectType const& t = *type;

  if (t.bounce > 0) {
    if (!game.level.Inside(inew_pos.x, ipos.y) || game.PixelMat(inew_pos.x, ipos.y).DirtRock()) {
      vel.x = -vel.x * t.bounce / 100;
      vel.y = (vel.y * 4) / 5;  // TODO: Read from EXE
      bounced = true;
    }

    if (!game.level.Inside(ipos.x, inew_pos.y) || game.PixelMat(ipos.x, inew_pos.y).DirtRock()) {
      vel.y = -vel.y * t.bounce / 100;
      vel.x = (vel.x * 4) / 5;  // TODO: Read from EXE
      bounced = true;
    }
  }

  if (t.blood_trail && t.blood_trail_delay > 0 && (game.cycles % t.blood_trail_delay) == 0) {
    game.CreateBObject(pos, vel / 4);  // TODO: Read from EXE
  }

  // Yes, we do this again.
  inew_pos = Ftoi(pos + vel);

  if (inew_pos.x < 0) {
    pos.x = 0;
  }
  if (inew_pos.y < 0) {
    pos.y = 0;
  }
  if (inew_pos.x >= game.level.width) {
    pos.x = Itof(game.level.width);
  }
  if (inew_pos.y >= game.level.height) {
    pos.y = Itof(game.level.height);
  }

  if (!game.level.Inside(inew_pos) || game.PixelMat(inew_pos.x, inew_pos.y).DirtRock()) {
    vel.Zero();

    if (t.expl_ground) {
      if (t.start_frame > 0 && t.draw_on_map) {
        BlitImageOnMap(common, game.level,
                       common.small_sprites.SpritePtr(t.start_frame + cur_frame), ipos.x - 3,
                       ipos.y - 3, 7, 7);
        if (game.settings->shadow) {
          CorrectShadow(common, game.level,
                        Rect(ipos.x - 8, ipos.y - 8, ipos.x + 9,
                             ipos.y + 9));  // This seems like an overly large rectangle
        }
      }

      do_explode = true;
    }
  } else {
    if (!bounced && t.leave_obj_delay != 0 &&
        t.leave_obj >= 0  // NOTE: AFAIK, this doesn't exist in Liero, but some TCs seem to forget
                          // to set leaveObjDelay to 0 when not using this trail
        && (game.cycles % t.leave_obj_delay) == 0) {
      common.sobject_types[t.leave_obj].Create(game, Ftoi(pos.x), Ftoi(pos.y), owner_idx, fired_by);
    }

    vel.y += t.gravity;
  }

  if (t.num_frames > 0) {
    if ((game.cycles & 7) == 0)  // TODO: Read from EXE
    {
      if (vel.x > 0) {
        ++cur_frame;
        if (cur_frame > t.num_frames) {
          cur_frame = 0;
        }
      } else if (vel.x < 0) {
        --cur_frame;
        if (cur_frame < 0) {
          cur_frame = t.num_frames;
        }
      }
    }
  }

  if (t.time_to_explo > 0) {
    if (--time_left <= 0) {
      do_explode = true;
    }
  }

  if (!do_explode) {
    if (t.hit_damage > 0) {
      for (std::size_t i = 0; i < game.worms.size(); ++i) {
        Worm& w = *game.worms[i];

        if (CheckForSpecWormHit(game, Ftoi(pos.x), Ftoi(pos.y), t.detect_distance, w)) {
          w.vel += vel * t.blow_away / 100;

          game.DoDamage(w, t.hit_damage, owner_idx);

          Worm* owner = game.WormByIdx(owner_idx);
          game.stats_recorder->DamageDealt(owner, fired_by, &w, t.hit_damage, has_hit);
          has_hit = true;

          if (t.hit_damage > 0 && w.health > 0 && game.rand(3) == 0) {
            int const kSnd =
                18 + game.rand(3);  // NOTE: MUST be outside the unpredictable branch below
            if (!game.sound_player->IsPlaying(&w)) {
              game.sound_player->Play(kSnd, &w);
            }
          }

          int const kBlood = t.blood_on_hit * game.settings->blood / 100;

          for (int i = 0; i < kBlood; ++i) {
            int const kAngle = game.rand(128);
            common.nobject_types[6].Create2(game, kAngle, vel / 3, pos, 0, owner_idx, nullptr);
          }

          if (t.worm_explode) {
            do_explode = true;
          } else if (t.worm_destroy && used) {
            game.nobjects.Free(this);
          }
        }
      }
    }
  }

  if (do_explode) {
    if (t.create_on_exp >= 0) {
      common.sobject_types[t.create_on_exp].Create(game, Ftoi(pos.x), Ftoi(pos.y), owner_idx,
                                                   fired_by);
    }

    if (t.dirt_effect >= 0) {
      DrawDirtEffect(common, game.rand, game.level, t.dirt_effect, Ftoi(pos.x) - 7,
                     Ftoi(pos.y) - 7);

      if (game.settings->shadow) {
        CorrectShadow(common, game.level,
                      Rect(Ftoi(pos.x) - 10, Ftoi(pos.y) - 10, Ftoi(pos.x) + 11, Ftoi(pos.y) + 11));
      }
    }

    if (t.splinter_amount > 0) {
      for (int i = 0; i < t.splinter_amount; ++i) {
        int const kAngle = game.rand(128);
        int const kColorSub = game.rand(2);
        common.nobject_types[t.splinter_type].Create2(
            game, kAngle, fixedvec(), pos, t.splinter_colour - kColorSub, owner_idx, nullptr);
      }
    }

    if (used) {
      game.nobjects.Free(this);
    }
  }
}
