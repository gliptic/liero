#include "spectatorviewport.hpp"
#include "constants.hpp"
#include "game.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/blit.hpp"
#include "gfx/renderer.hpp"
#include "math.hpp"
#include "text.hpp"

void SpectatorViewport::Process(Game& game) {
  int real_shake = Ftoi(shake);

  // FIXME this is a bit broken
  if (game.WormByIdx(0)->killed_timer == Worm::kKilledTimerInitial ||
      game.WormByIdx(1)->killed_timer == Worm::kKilledTimerInitial) {
    banner_y = -8;
  }

  if (real_shake > 0) {
    x += rand(real_shake * 2) - real_shake;
    y += rand(real_shake * 2) - real_shake;
  }

  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > max_x) x = max_x;
  if (y > max_y) y = max_y;
}

void SpectatorViewport::Draw(Game& game, Renderer& renderer, GameState state, bool is_replay) {
  Common& common = *game.common;
  int multiplier = renderer.render_res_x / 320;
  int center_x = renderer.render_res_x / 2;
  IVec2 render_pos(x, y);
  fixedvec offs = rect.Ul() - render_pos;

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    int offset_x = offs.x / (i + 1);
    // fix misalignment. Not sure why this is needed
    if (i == 1) {
      offset_x += 2;
    }
    int offset_weapon_list_x = center_x - 15 + (i == 0 ? -90 : 60);
    if (worm.visible) {
      int lifebar_width = worm.health * 100 / worm.settings->health;
      DrawBar(renderer.bmp, offset_x + worm.stats_x * multiplier, renderer.render_res_y - 39,
              lifebar_width, lifebar_width / 10 + 234);
    } else {
      int lifebar_width = 100 - (worm.killed_timer * 25) / 37;
      if (lifebar_width > 0) {
        if (lifebar_width > 100) lifebar_width = 100;
        DrawBar(renderer.bmp, offset_x + worm.stats_x * multiplier, renderer.render_res_y - 39,
                lifebar_width, lifebar_width / 10 + 234);
      }
    }

    // Draw kills status

    WormWeapon const& ww = worm.weapons[worm.current_weapon];

    if (ww.Available()) {
      if (ww.ammo > 0) {
        int ammo_bar_width = ww.ammo * 100 / ww.type->ammo;

        if (ammo_bar_width > 0)
          DrawBar(renderer.bmp, offset_x + worm.stats_x * multiplier, renderer.render_res_y - 34,
                  ammo_bar_width, ammo_bar_width / 10 + 245);
      }
    } else {
      int ammo_bar_width = 0;

      if (ww.type->loading_time != 0) {
        int computed_loading_time = ww.type->ComputedLoadingTime(*game.settings);
        ammo_bar_width = 100 - ww.loading_left * 100 / computed_loading_time;
      } else {
        ammo_bar_width = 100 - ww.loading_left * 100;
      }

      if (ammo_bar_width > 0)
        DrawBar(renderer.bmp, offset_x + worm.stats_x * multiplier, renderer.render_res_y - 34,
                ammo_bar_width, ammo_bar_width / 10 + 245);

      if ((game.cycles % 20) > 10 && worm.visible) {
        common.font.DrawString(renderer.bmp, LS(Reloading), offset_x + worm.stats_x * multiplier,
                               164, 50);
      }
    }

    common.font.DrawString(renderer.bmp, (LS(Kills) + ToString(worm.kills)),
                           offset_x + worm.stats_x * multiplier, renderer.render_res_y - 29, 10);

    // always display player names, color and time in spectator view
    common.font.DrawString(renderer.bmp, worm.settings->name, offset_x + worm.stats_x * multiplier,
                           renderer.render_res_y - 15, 7);
    FillRect(renderer.bmp, offset_x + worm.stats_x * multiplier - 1, renderer.render_res_y - 7 - 1,
             8, 8, 7);
    FillRect(renderer.bmp, offset_x + worm.stats_x * multiplier, renderer.render_res_y - 7, 6, 6,
             worm.settings->color);
    // time
    // FIXME: only draw this once, not once per worm
    common.font.DrawString(renderer.bmp, TimeToStringEx(game.cycles * 14, false, true),
                           center_x - 15, renderer.render_res_y - 15, 7);

    // draw available/selected weapons
    if (state == kStateGame) {
      for (int i = 0; i < 5; i++) {
        WormWeapon const& ww = worm.weapons[i];
        if (ww.ammo > 0) {
          int ammo_bar_width = ww.ammo * 60 / ww.type->ammo;

          if (ammo_bar_width > 0) {
            DrawBar(renderer.bmp, offset_weapon_list_x, renderer.render_res_y - 40 + i * 8,
                    ammo_bar_width, 5, ammo_bar_width / 6 + 245);
          }
        }
        if (worm.current_weapon == i) {
          common.font.DrawString(renderer.bmp, worm.weapons[i].type->name, offset_weapon_list_x,
                                 renderer.render_res_y - 40 + i * 8, 187);
        } else {
          common.font.DrawString(renderer.bmp, worm.weapons[i].type->name, offset_weapon_list_x,
                                 renderer.render_res_y - 40 + i * 8, 185);
        }
      }
    }

    int const kStateColours[2][2] = {{6, 10}, {79, 4}};

    switch (game.settings->game_mode) {
      case Settings::kGmKillEmAll:
      case Settings::kGmScalesOfJustice: {
        common.font.DrawString(renderer.bmp, (LS(Lives) + ToString(worm.lives)),
                               offset_x + worm.stats_x * multiplier, renderer.render_res_y - 22, 6);
      } break;

      case Settings::kGmHoldazone: {
        int state = 0;

        for (auto const& w : game.worms)
          if (w.get() != &worm && w->timer <= worm.timer) state = 1;

        int color = kStateColours[game.holdazone.holder_idx != worm.index][state];

        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, color);
      } break;

      case Settings::kGmGameOfTag: {
        int state = 0;

        for (auto const& w : game.worms)
          if (w.get() != &worm && w->timer >= worm.timer) state = 1;

        int color = kStateColours[game.last_killed_idx != worm.index][state];

        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, color);
      } break;
    }
  }

  BlitImageNoKeyColour(renderer.bmp, &game.level.data[0], offs.x, offs.y, game.level.width,
                       game.level.height);

  if (game.settings->game_mode == Settings::kGmHoldazone) {
    bool timing_out = game.holdazone.timeout_left < 70 * 4;

    int color = timing_out ? 168 : 50;
    int contender_color;

    if (game.holdazone.contender_idx >= 0) {
      Worm* contender = game.WormByIdx(game.holdazone.contender_idx);
      if (timing_out)
        contender_color = contender->MinimapColor();
      else
        contender_color = Palette::kWormColourIndexes[contender->index] + 5;
    } else {
      contender_color = color;
    }

    DrawDashedLineBox(renderer.bmp, game.holdazone.rect.x1 + offs.x,
                      game.holdazone.rect.y1 + offs.y, color, contender_color,
                      game.holdazone.contender_frames, game.settings->kZoneCaptureTime,
                      game.holdazone.rect.Width(), game.holdazone.rect.Height(), game.cycles / 10);
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    if (!worm.visible && worm.killed_timer <= 0 && !worm.ready) {
      if (game.settings->allow_viewing_spawn_point && worm.Pressed(Worm::kChange)) {
        int temp_x = Ftoi(worm.pos.x) - 7 + offs.x;
        int temp_y = Ftoi(worm.pos.y) - 5 + offs.y;

        BlitImageTrans(renderer.bmp,
                       common.WormSpriteObj(worm.current_frame, worm.direction, worm.index), temp_x,
                       temp_y, game.cycles);
      }
    }

    if (banner_y > -8 && worm.health <= 0) {
      if (game.settings->game_mode == Settings::kGmGameOfTag && game.got_changed) {
        common.font.DrawString(renderer.bmp, LS(YoureIt), rect.x1 + 3, banner_y + 1, 0);
        common.font.DrawString(renderer.bmp, LS(YoureIt), rect.x1 + 2, banner_y, 50);
      }
    }
  }
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    if (worm.health <= 0 && banner_y > -8) {
      if (worm.last_killed_by_idx == worm.index) {
        std::string msg(worm.settings->name + LS(CommittedSuicideMsg));
        common.font.DrawString(renderer.bmp, msg, rect.x1 + 3, banner_y + 1, 0);
        common.font.DrawString(renderer.bmp, msg, rect.x1 + 2, banner_y, 50);
      } else {
        std::string msg(game.worms[worm.last_killed_by_idx]->settings->name + " killed " +
                        worm.settings->name);
        common.font.DrawString(renderer.bmp, msg, rect.x1 + 3, banner_y + 1, 0);
        common.font.DrawString(renderer.bmp, msg, rect.x1 + 2, banner_y, 50);
      }
    }
  }

  auto br = game.bonuses.All();
  for (Bonus* i; (i = br.Next());) {
    if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
      int f = common.bonus_frames[i->frame];

      BlitImage(renderer.bmp, common.small_sprites[f], Ftoi(i->x) - 3 + offs.x,
                Ftoi(i->y) - 3 + offs.y);

      if (game.settings->shadow) {
        BlitShadowImage(common, renderer.bmp, common.small_sprites.SpritePtr(f),
                        Ftoi(i->x) - 5 + offs.x,  // TODO: Use offsX
                        Ftoi(i->y) - 1 + offs.y, 7, 7);
      }

      if (game.settings->names_on_bonuses && i->frame == 0) {
        std::string const& name = common.weapons[i->weapon].name;
        int len = int(name.size()) * 4;

        common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(i->x) - len / 2 + offs.x,
                             Ftoi(i->y) - 10 + offs.y);
      }
    }
  }

  auto sr = game.sobjects.All();
  for (SObject* i; (i = sr.Next());) {
    SObjectType const& t = common.sobject_types[i->id];
    int frame = i->cur_frame + t.start_frame;

    BlitImageR(renderer.bmp, common.large_sprites.SpritePtr(frame), i->x + offs.x, i->y + offs.y,
               16, 16);

    if (game.settings->shadow) {
      BlitShadowImage(common, renderer.bmp, common.large_sprites.SpritePtr(frame),
                      i->x + offs.x - 3,
                      i->y + offs.y + 3,  // TODO: Original doesn't offset the shadow, which is
                                          // clearly wrong. Check that this offset is correct.
                      16, 16);
    }
  }

  auto wr = game.wobjects.All();
  for (WObject* i; (i = wr.Next());) {
    Weapon const& w = *i->type;

    if (w.start_frame > -1) {
      int cur_frame = i->cur_frame;
      int shot_type = w.shot_type;

      if (shot_type == 2) {
        cur_frame += 4;
        cur_frame >>= 3;
        if (cur_frame < 0)
          cur_frame = 16;
        else if (cur_frame > 15)
          cur_frame -= 16;
      } else if (shot_type == 3) {
        if (cur_frame > 64) --cur_frame;
        cur_frame -= 12;
        cur_frame >>= 3;
        if (cur_frame < 0)
          cur_frame = 0;
        else if (cur_frame > 12)
          cur_frame = 12;
      }

      int pos_x = Ftoi(i->pos.x) - 3;
      int pos_y = Ftoi(i->pos.y) - 3;

      if (game.settings->shadow && w.shadow) {
        BlitShadowImage(common, renderer.bmp,
                        common.small_sprites.SpritePtr(w.start_frame + cur_frame),
                        pos_x - 3 + offs.x, pos_y + 3 + offs.y, 7, 7);
      }

      BlitImage(renderer.bmp, common.small_sprites[w.start_frame + cur_frame], pos_x + offs.x,
                pos_y + offs.y);
    } else if (i->cur_frame > 0) {
      int pos_x = Ftoi(i->pos.x) - x + rect.x1;
      int pos_y = Ftoi(i->pos.y) - y + rect.y1;

      if (renderer.bmp.clip_rect.Inside(pos_x, pos_y))
        renderer.bmp.GetPixel(pos_x, pos_y) = static_cast<PalIdx>(i->cur_frame);

      if (game.settings->shadow) {
        pos_x -= 3;
        pos_y += 3;

        if (renderer.bmp.clip_rect.Inside(pos_x, pos_y)) {
          PalIdx& pix = renderer.bmp.GetPixel(pos_x, pos_y);
          if (common.materials[pix].SeeShadow()) pix += 4;
        }
      }
    }

    if (!common.h[HRemExp] && i->type - &common.weapons[0] == 34 &&
        game.settings->names_on_bonuses)  // TODO: Read from EXE
    {
      if (i->cur_frame == 0) {
        int name_num = int(&*i - game.wobjects.arr) %
                       (int)common.weapons.size();  // TODO: Something nicer maybe

        std::string const& name = common.weapons[name_num].name;
        int width = int(name.size()) * 4;

        common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(i->pos.x) - width / 2 + offs.x,
                             Ftoi(i->pos.y) - 10 + offs.y);
      }
    }
  }

  auto nr = game.nobjects.All();
  for (NObject* i; (i = nr.Next());) {
    NObjectType const& t = *i->type;

    if (t.start_frame > 0) {
      auto pos = Ftoi(i->pos) - IVec2(3, 3);

      if (game.settings->shadow) {
        BlitShadowImage(common, renderer.bmp,
                        common.small_sprites.SpritePtr(t.start_frame + i->cur_frame),
                        pos.x - 3 + offs.x, pos.y + 3 + offs.y, 7, 7);
      }

      BlitImage(renderer.bmp, common.small_sprites[t.start_frame + i->cur_frame], pos.x + offs.x,
                pos.y + offs.y);

    } else if (i->cur_frame > 1) {
      auto pos = Ftoi(i->pos) + offs;
      if (renderer.bmp.clip_rect.Encloses(pos))
        renderer.bmp.GetPixel(pos.x, pos.y) = PalIdx(i->cur_frame);

      if (game.settings->shadow) {
        pos.x -= 3;
        pos.y += 3;

        if (renderer.bmp.clip_rect.Encloses(pos)) {
          PalIdx& pix = renderer.bmp.GetPixel(pos.x, pos.y);
          if (common.materials[pix].SeeShadow()) pix += 4;
        }
      }
    }
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& w = *game.worms[i];

    if (w.visible) {
      int temp_x = Ftoi(w.pos.x) - 7 + offs.x;
      int temp_y = Ftoi(w.pos.y) - 5 + offs.y;
      int angle_frame = w.AngleFrame();

      if (w.weapons[w.current_weapon].Available()) {
        int hotspot_x = w.hotspot_x + offs.x;
        int hotspot_y = w.hotspot_y + offs.y;

        WormWeapon const& ww = w.weapons[w.current_weapon];
        Weapon const& weapon = *ww.type;

        if (weapon.laser_sight) {
          DrawLaserSight(renderer.bmp, rand, hotspot_x, hotspot_y, temp_x + 7, temp_y + 4);
        }

        if (ww.type - &common.weapons[0] == LC(LaserWeapon) - 1 && w.Pressed(Worm::kFire)) {
          DrawLine(renderer.bmp, hotspot_x, hotspot_y, temp_x + 7, temp_y + 4,
                   weapon.color_bullets);
        }
      }

      if (w.ninjarope.out) {
        int ninjarope_x = Ftoi(w.ninjarope.pos.x) + offs.x;
        int ninjarope_y = Ftoi(w.ninjarope.pos.y) + offs.y;

        DrawNinjarope(common, renderer.bmp, ninjarope_x, ninjarope_y, temp_x + 7, temp_y + 4);

        BlitImage(renderer.bmp, common.large_sprites[84], ninjarope_x - 1, ninjarope_y - 1);

        if (game.settings->shadow) {
          DrawShadowLine(common, renderer.bmp, ninjarope_x - 3, ninjarope_y + 3, temp_x + 7 - 3,
                         temp_y + 4 + 3);
          BlitShadowImage(common, renderer.bmp, common.large_sprites.SpritePtr(84), ninjarope_x - 4,
                          ninjarope_y + 2, 16, 16);
        }
      }

      if (w.weapons[w.current_weapon].type->fire_cone > 0 && w.fire_cone > 0) {
        /* TODO
        //NOTE! Check fctab so it's correct
        //NOTE! Check function 1071C and see what it actually does*/

        BlitFireCone(renderer.bmp, w.fire_cone / 2, common.FireConeSprite(angle_frame, w.direction),
                     common.fire_cone_offset[w.direction][angle_frame][0] + temp_x,
                     common.fire_cone_offset[w.direction][angle_frame][1] + temp_y);
      }

      BlitImage(renderer.bmp, common.WormSpriteObj(w.current_frame, w.direction, w.index), temp_x,
                temp_y);
      if (game.settings->shadow)
        BlitShadowImage(common, renderer.bmp,
                        common.WormSprite(w.current_frame, w.direction, w.index), temp_x - 3,
                        temp_y + 3, 16, 16);
    }

    if (w.ai) w.ai->DrawDebug(game, w, renderer, offs.x, offs.y);
  }

  /*
  auto& dp = gfx.debugPoints;

  for (auto& p : dp)
  {
          int x = ftoi(p.first) + offsX;
          int y = ftoi(p.second) + offsY;

          if(isInside(renderer.bmp.clip_rect, x, y))
                  renderer.bmp.getPixel(x, y) = 0;
  }*/

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    if (worm.visible) {
      auto temp =
          Ftoi(worm.pos) - IVec2(1, 2) + Ftoi(cossin_table[Ftoi(worm.aiming_angle)] * 16) + offs;
      // int tempX = ftoi(worm.pos.x) - 1 + ftoi(cosTable[ftoi(worm.aimingAngle)] * 16) + offs.x;
      // int tempY = ftoi(worm.pos.y) - 2 + ftoi(sinTable[ftoi(worm.aimingAngle)] * 16) + offs.y;

      BlitImage(renderer.bmp, common.small_sprites[worm.make_sight_green ? 44 : 43], temp.x,
                temp.y);

      if (worm.Pressed(Worm::kChange)) {
        std::string const& name = worm.weapons[worm.current_weapon].type->name;

        int len = int(name.size()) * 4;  // TODO: Read 4 from exe? (SW_CHARWID)

        common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(worm.pos.x) - len / 2 + 1 + offs.x,
                             Ftoi(worm.pos.y) - 10 + offs.y);
      }
    }
  }

  for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
    auto ipos = Ftoi(i->pos) + offs;
    if (renderer.bmp.clip_rect.Encloses(ipos))
      renderer.bmp.GetPixel(ipos.x, ipos.y) = PalIdx(i->color);

    if (game.settings->shadow) {
      ipos.x -= 3;
      ipos.y += 3;

      if (renderer.bmp.clip_rect.Encloses(ipos)) {
        PalIdx& pix = renderer.bmp.GetPixel(ipos.x, ipos.y);
        if (common.materials[pix].SeeShadow()) pix += 4;
      }
    }
  }
}
