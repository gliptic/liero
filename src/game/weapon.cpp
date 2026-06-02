#include "weapon.hpp"
#include "constants.hpp"
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "math.hpp"
#include "mixer/player.hpp"

int Weapon::ComputedLoadingTime(Settings& settings) const {
  int ret = (settings.loading_time * loading_time) / 100;
  if (ret == 0) ret = 1;
  return ret;
}

void Weapon::Fire(Game& game, int angle, fixedvec vel, int speed, fixedvec pos, int owner_idx,
                  WormWeapon* ww) const {
  WObject* obj = game.wobjects.NewObjectReuse();

  obj->type = this;
  obj->pos = pos;
  obj->owner_idx = owner_idx;

  // STATS
  obj->fired_by = ww;
  obj->has_hit = false;

  Worm* owner = game.WormByIdx(owner_idx);
  game.stats_recorder->DamagePotential(owner, ww, hit_damage);
  game.stats_recorder->Shot(owner, ww);

  obj->vel = cossin_table[angle] * speed / 100 + vel;

  if (distribution) {
    obj->vel.x += game.rand(distribution * 2) - distribution;
    obj->vel.y += game.rand(distribution * 2) - distribution;
  }

  if (start_frame >= 0) {
    if (shot_type == kStNormal) {
      if (loop_anim) {
        if (num_frames)
          obj->cur_frame = game.rand(num_frames + 1);
        else
          obj->cur_frame = game.rand(2);
      } else
        obj->cur_frame = 0;
    } else if (shot_type == kStdType1) {
      if (angle > 64) --angle;

      int cur_frame = (angle - 12) >> 3;
      if (cur_frame < 0)
        cur_frame = 0;
      else if (cur_frame > 12)
        cur_frame = 12;
      obj->cur_frame = cur_frame;
    } else if (shot_type == kStdType2 || shot_type == kStSteerable) {
      obj->cur_frame = angle;
    } else
      obj->cur_frame = 0;
  } else {
    obj->cur_frame = color_bullets - game.rand(2);
  }

  obj->time_left = time_to_explo;

  if (time_to_explo_v) obj->time_left -= game.rand(time_to_explo_v);
}

void WObject::BlowUpObject(Game& game, int cause_idx) {
  Common& common = *game.common;
  Weapon const& w = *type;

  fixed x = this->pos.x;
  fixed y = this->pos.y;
  fixed vel_x = this->vel.x;
  fixed vel_y = this->vel.y;

  game.wobjects.Free(this);

  if (w.create_on_exp >= 0) {
    common.sobject_types[w.create_on_exp].Create(game, Ftoi(x), Ftoi(y), cause_idx, fired_by, this);
  }

  game.sound_player->Play(w.explo_sound);

  int splinters = w.splinter_amount;

  if (splinters > 0) {
    if (w.splinter_scatter == 0) {
      for (int i = 0; i < splinters; ++i) {
        int angle = game.rand(128);
        int color_sub = game.rand(2);
        common.nobject_types[w.splinter_type].Create2(game, angle, fixedvec(), fixedvec(x, y),
                                                      w.splinter_colour - color_sub, cause_idx,
                                                      fired_by);
      }
    } else {
      for (int i = 0; i < splinters; ++i) {
        int color_sub = game.rand(2);
        common.nobject_types[w.splinter_type].Create1(game, fixedvec(vel_x, vel_y), fixedvec(x, y),
                                                      w.splinter_colour - color_sub, cause_idx,
                                                      fired_by);
      }
    }
  }

  if (w.dirt_effect >= 0) {
    int ix = Ftoi(x), iy = Ftoi(y);
    DrawDirtEffect(common, game.rand, game.level, w.dirt_effect, Ftoi(x) - 7, Ftoi(y) - 7);
    if (game.settings->shadow)
      CorrectShadow(common, game.level, Rect(ix - 10, iy - 10, ix + 11, iy + 11));
  }
}

void WObject::Process(Game& game) {
  int iter = 0;
  bool do_explode = false;
  bool do_remove = false;

  Common& common = *game.common;
  Weapon const& w = *type;

  Worm* owner = game.WormByIdx(owner_idx);

  // As liero would do this while rendering, we try to do it as early as possible
  if (common.h[HRemExp] && type - &common.weapons[0] == LC(RemExpObject) - 1) {
    if (owner->Pressed(Worm::kChange) && owner->Pressed(Worm::kFire)) {
      time_left = 0;
    }
  }

  do {
    ++iter;
    pos += vel;

    if (w.shot_type == 2) {
      fixedvec dir(cossin_table[cur_frame]);
      auto new_vel = dir * w.speed / 100;

      if (owner->visible && owner->Pressed(Worm::kUp)) {
        new_vel += dir * w.add_speed / 100;
      }

      vel = ((vel * 8) + new_vel) / 9;
    } else if (w.shot_type == 3) {
      fixedvec dir(cossin_table[cur_frame]);
      auto add_vel = dir * w.add_speed / 100;

      vel += add_vel;

      if (w.distribution) {
        vel.x += game.rand(w.distribution * 2) - w.distribution;
        vel.y += game.rand(w.distribution * 2) - w.distribution;
      }
    }

    if (w.bounce > 0) {
      auto ipos = Ftoi(pos);
      auto inew_pos = Ftoi(pos + vel);

      if (!game.level.Inside(inew_pos.x, ipos.y) || game.PixelMat(inew_pos.x, ipos.y).DirtRock()) {
        if (w.bounce != 100) {
          vel.x = -vel.x * w.bounce / 100;
          vel.y = (vel.y * 4) / 5;  // TODO: Read from EXE
        } else
          vel.x = -vel.x;
      }

      if (!game.level.Inside(ipos.x, inew_pos.y) || game.PixelMat(ipos.x, inew_pos.y).DirtRock()) {
        if (w.bounce != 100) {
          vel.y = -vel.y * w.bounce / 100;
          vel.x = (vel.x * 4) / 5;  // TODO: Read from EXE
        } else
          vel.y = -vel.y;
      }
    }

    if (w.mult_speed != 100) {
      vel = vel * w.mult_speed / 100;
    }

    if (w.obj_trail_type >= 0 && (game.cycles % w.obj_trail_delay) == 0) {
      common.sobject_types[w.obj_trail_type].Create(game, Ftoi(pos.x), Ftoi(pos.y), owner_idx,
                                                    fired_by);
    }

    if (w.part_trail_obj >= 0 && (game.cycles % w.part_trail_delay) == 0) {
      if (w.part_trail_type == 1) {
        common.nobject_types[w.part_trail_obj].Create1(game, vel / LC(SplinterLarpaVelDiv), pos, 0,
                                                       owner_idx, fired_by);
      } else {
        int angle = game.rand(128);
        common.nobject_types[w.part_trail_obj].Create2(
            game, angle, vel / LC(SplinterCracklerVelDiv), pos, 0, owner_idx, fired_by);
      }
    }

    if (w.collide_with_objects) {
      auto impulse = vel * w.blow_away / 100;

      auto wr = game.wobjects.All();
      for (WObject* i; (i = wr.Next());) {
        if (i->type != type || i->owner_idx != owner_idx) {
          if (pos.x >= i->pos.x - Itof(2) && pos.x <= i->pos.x + Itof(2) &&
              pos.y >= i->pos.y - Itof(2) && pos.y <= i->pos.y + Itof(2)) {
            i->vel += impulse;
          }
        }
      }

      auto nr = game.nobjects.All();
      for (NObject* i; (i = nr.Next());) {
        if (pos.x >= i->pos.x - Itof(2) && pos.x <= i->pos.x + Itof(2) &&
            pos.y >= i->pos.y - Itof(2) && pos.y <= i->pos.y + Itof(2)) {
          i->vel += impulse;
        }
      }
    }

    auto inew_pos = Ftoi(pos + vel);

    if (inew_pos.x < 0) pos.x = 0;
    if (inew_pos.y < 0) pos.y = 0;
    if (inew_pos.x >= game.level.width) pos.x = Itof(game.level.width - 1);
    if (inew_pos.y >= game.level.height) pos.y = Itof(game.level.height - 1);

    if (!game.level.Inside(inew_pos) || game.PixelMat(inew_pos.x, inew_pos.y).DirtRock()) {
      if (w.bounce == 0) {
        if (w.expl_ground) {
          do_explode = true;
        } else {
          vel.Zero();
        }
      }
    } else {
      vel.y += w.gravity;  // The original tested w.gravity first, which doesn't seem like a gain

      if (w.num_frames > 0) {
        if ((game.cycles & 7) == 0) {
          if (!w.loop_anim) {
            if (++cur_frame > w.num_frames) cur_frame = 0;
          } else {
            if (vel.x < 0) {
              if (--cur_frame < 0) cur_frame = w.num_frames;
            } else if (vel.x > 0) {
              if (++cur_frame > w.num_frames) cur_frame = 0;
            }
          }
        }
      }
    }

    if (w.time_to_explo > 0) {
      if (--time_left < 0) do_explode = true;
    }

    for (std::size_t i = 0; i < game.worms.size(); ++i) {
      Worm& worm = *game.worms[i];

      if ((w.hit_damage || w.blow_away || w.blood_on_hit || w.worm_collide) &&
          CheckForSpecWormHit(game, Ftoi(pos.x), Ftoi(pos.y), w.detect_distance, worm)) {
        worm.vel += vel * w.blow_away / 100;

        game.DoDamage(worm, w.hit_damage, owner_idx);
        game.stats_recorder->DamageDealt(owner, fired_by, &worm, w.hit_damage, has_hit);
        if (!has_hit) game.stats_recorder->Hit(owner, fired_by, &worm);
        has_hit = true;

        int blood_amount = w.blood_on_hit * game.settings->blood / 100;

        for (int i = 0; i < blood_amount; ++i) {
          int angle = game.rand(128);
          common.nobject_types[6].Create2(game, angle, vel / 3, pos, 0, worm.index, fired_by);
        }

        if (w.hit_damage > 0 && worm.health > 0 && game.rand(3) == 0) {
          int snd = game.rand(3) + 18;  // NOTE: MUST be outside the unpredictable branch below
          if (!game.sound_player->IsPlaying(&worm)) {
            game.sound_player->Play(snd, &worm);
          }
        }

        if (w.worm_collide) {
          if (game.rand(w.worm_collide) == 0) {
            if (w.worm_explode) do_explode = true;

            do_remove = true;
          }
        }
      }
    }

    if (do_explode) {
      BlowUpObject(game, owner_idx);
      break;
    } else if (do_remove) {
      game.wobjects.Free(this);
      break;
    }
  } while (w.shot_type == Weapon::kStLaser && used  // TEMP
           && (iter < 8 || w.id == 28));
}
