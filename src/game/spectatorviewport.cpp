#include "spectatorviewport.hpp"

#include <algorithm>
#include "constants.hpp"
#include "game.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/blit.hpp"
#include "gfx/renderer.hpp"
#include "gfx/shadow_query.hpp"
#include "math.hpp"
#include "text.hpp"

void SpectatorViewport::Process(Game& game) {
  int const kRealShake = Ftoi(shake);

  // FIXME this is a bit broken
  if (game.WormByIdx(0)->killed_timer == Worm::kKilledTimerInitial ||
      game.WormByIdx(1)->killed_timer == Worm::kKilledTimerInitial) {
    banner_y = -8;
  }

  if (kRealShake > 0) {
    x += rand(kRealShake * 2) - kRealShake;
    y += rand(kRealShake * 2) - kRealShake;
  }

  x = std::max(x, 0);
  y = std::max(y, 0);
  x = std::min(x, max_x);
  y = std::min(y, max_y);
}

void SpectatorViewport::Draw(Game& game, Renderer& renderer, GameState state, bool /*is_replay*/) {
  Common& common = *game.common;
  int const kMultiplier = renderer.render_res_x / 320;
  int const kCenterX = renderer.render_res_x / 2;
  IVec2 const kRenderPos(x, y);
  fixedvec const kOffs = rect.Ul() - kRenderPos;

  // Shadows and explosion masks query the level (screen + offset = world).
  ShadowQuery const kShadow{.common = common,
                            .level = game.level,
                            .pal32 = renderer.pal32,
                            .world_offset_x = -kOffs.x,
                            .world_offset_y = -kOffs.y,
                            .mode = renderer.mode};

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    int offset_x = kOffs.x / (i + 1);
    // fix misalignment. Not sure why this is needed
    if (i == 1) {
      offset_x += 2;
    }
    int const kOffsetWeaponListX = kCenterX - 15 + (i == 0 ? -90 : 60);
    if (worm.visible) {
      int const kLifebarWidth = worm.health * 100 / worm.settings->health;
      DrawBar(renderer.bmp, offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 39,
              kLifebarWidth, kLifebarWidth / 10 + 234);
    } else {
      int lifebar_width = 100 - (worm.killed_timer * 25) / 37;
      if (lifebar_width > 0) {
        lifebar_width = std::min(lifebar_width, 100);
        DrawBar(renderer.bmp, offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 39,
                lifebar_width, lifebar_width / 10 + 234);
      }
    }

    // Draw kills status

    WormWeapon const& ww = worm.weapons[worm.current_weapon];

    if (ww.Available()) {
      if (ww.ammo > 0) {
        int const kAmmoBarWidth = ww.ammo * 100 / ww.type->ammo;

        if (kAmmoBarWidth > 0) {
          DrawBar(renderer.bmp, offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 34,
                  kAmmoBarWidth, kAmmoBarWidth / 10 + 245);
        }
      }
    } else {
      int ammo_bar_width = 0;

      if (ww.type->loading_time != 0) {
        int const kComputedLoadingTime = ww.type->ComputedLoadingTime(*game.settings);
        ammo_bar_width = 100 - ww.loading_left * 100 / kComputedLoadingTime;
      } else {
        ammo_bar_width = 100 - ww.loading_left * 100;
      }

      if (ammo_bar_width > 0) {
        DrawBar(renderer.bmp, offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 34,
                ammo_bar_width, ammo_bar_width / 10 + 245);
      }

      if ((game.cycles % 20) > 10 && worm.visible) {
        common.font.DrawString(renderer.bmp, LS(Reloading), offset_x + worm.stats_x * kMultiplier,
                               164, 50);
      }
    }

    common.font.DrawString(renderer.bmp, (LS(Kills) + ToString(worm.kills)),
                           offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 29, 10);

    // always display player names, color and time in spectator view
    common.font.DrawString(renderer.bmp, worm.settings->name, offset_x + worm.stats_x * kMultiplier,
                           renderer.render_res_y - 15, 7);
    FillRect(renderer.bmp, offset_x + worm.stats_x * kMultiplier - 1, renderer.render_res_y - 7 - 1,
             8, 8, 7);
    FillRect(renderer.bmp, offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 7, 6, 6,
             worm.settings->color);
    // time
    // FIXME: only draw this once, not once per worm
    common.font.DrawString(
        renderer.bmp,
        TimeToStringEx(game.cycles * 14, /*force_hours=*/false, /*force_minutes=*/true),
        kCenterX - 15, renderer.render_res_y - 15, 7);

    // draw available/selected weapons
    if (state == kStateGame) {
      for (int i = 0; i < 5; i++) {
        WormWeapon const& ww = worm.weapons[i];
        if (ww.ammo > 0) {
          int const kAmmoBarWidth = ww.ammo * 60 / ww.type->ammo;

          if (kAmmoBarWidth > 0) {
            DrawBar(renderer.bmp, kOffsetWeaponListX, renderer.render_res_y - 40 + i * 8,
                    kAmmoBarWidth, 5, kAmmoBarWidth / 6 + 245);
          }
        }
        if (worm.current_weapon == i) {
          common.font.DrawString(renderer.bmp, worm.weapons[i].type->name, kOffsetWeaponListX,
                                 renderer.render_res_y - 40 + i * 8, 187);
        } else {
          common.font.DrawString(renderer.bmp, worm.weapons[i].type->name, kOffsetWeaponListX,
                                 renderer.render_res_y - 40 + i * 8, 185);
        }
      }
    }

    int const kStateColours[2][2] = {{6, 10}, {79, 4}};

    switch (game.settings->game_mode) {
      case Settings::kGmKillEmAll:
      case Settings::kGmScalesOfJustice: {
        common.font.DrawString(renderer.bmp, (LS(Lives) + ToString(worm.lives)),
                               offset_x + worm.stats_x * kMultiplier, renderer.render_res_y - 22,
                               6);
      } break;

      case Settings::kGmHoldazone: {
        int state = 0;

        for (auto const& w : game.worms) {
          if (w.get() != &worm && w->timer <= worm.timer) {
            state = 1;
          }
        }

        int const kColor = kStateColours[game.holdazone.holder_idx != worm.index][state];

        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, kColor);
      } break;

      case Settings::kGmGameOfTag: {
        int state = 0;

        for (auto const& w : game.worms) {
          if (w.get() != &worm && w->timer >= worm.timer) {
            state = 1;
          }
        }

        int const kColor = kStateColours[game.last_killed_idx != worm.index][state];

        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, kColor);
      } break;

      default:
        break;
    }
  }

  DrawLevel(renderer.bmp, game.level, kOffs.x, kOffs.y);

  if (game.settings->game_mode == Settings::kGmHoldazone) {
    bool const kTimingOut = game.holdazone.timeout_left < 70 * 4;

    int const kColor = kTimingOut ? 168 : 50;
    int contender_color = 0;

    if (game.holdazone.contender_idx >= 0) {
      Worm const* contender = game.WormByIdx(game.holdazone.contender_idx);
      if (kTimingOut) {
        contender_color = contender->MinimapColor();
      } else {
        contender_color = Palette::kWormColorBlocks[contender->index].colour_index + 5;
      }
    } else {
      contender_color = kColor;
    }

    DrawDashedLineBox(renderer.bmp, game.holdazone.rect.x1 + kOffs.x,
                      game.holdazone.rect.y1 + kOffs.y, kColor, contender_color,
                      game.holdazone.contender_frames, Settings::kZoneCaptureTime,
                      game.holdazone.rect.Width(), game.holdazone.rect.Height(), game.cycles / 10);
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    if (!worm.visible && worm.killed_timer <= 0 && !worm.ready) {
      if (game.settings->allow_viewing_spawn_point && worm.Pressed(Worm::kChange)) {
        int const kTempX = Ftoi(worm.pos.x) - 7 + kOffs.x;
        int const kTempY = Ftoi(worm.pos.y) - 5 + kOffs.y;

        BlitImageTrans(renderer.bmp,
                       common.WormSpriteObj(worm.current_frame, worm.direction, worm.index), kTempX,
                       kTempY, game.cycles);
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
        std::string const kMsg(worm.settings->name + LS(CommittedSuicideMsg));
        common.font.DrawString(renderer.bmp, kMsg, rect.x1 + 3, banner_y + 1, 0);
        common.font.DrawString(renderer.bmp, kMsg, rect.x1 + 2, banner_y, 50);
      } else {
        std::string const kMsg(game.worms[worm.last_killed_by_idx]->settings->name + " killed " +
                               worm.settings->name);
        common.font.DrawString(renderer.bmp, kMsg, rect.x1 + 3, banner_y + 1, 0);
        common.font.DrawString(renderer.bmp, kMsg, rect.x1 + 2, banner_y, 50);
      }
    }
  }

  // Pass 1: all shadows. Every shadow is drawn before any sprite so no
  // object's shadow can overwrite another object's already-rendered sprite.
  if (game.settings->shadow) {
    {
      auto br = game.bonuses.All();
      for (Bonus const* i = nullptr; (i = br.Next());) {
        if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
          int const kF = common.bonus_frames[i->frame];
          BlitShadowImage(kShadow, renderer.bmp, common.small_sprites.SpritePtr(kF),
                          Ftoi(i->x) - 5 + kOffs.x,  // TODO: Use offsX
                          Ftoi(i->y) - 1 + kOffs.y, 7, 7);
        }
      }
    }

    {
      auto sr = game.sobjects.All();
      for (SObject const* i = nullptr; (i = sr.Next());) {
        SObjectType const& t = common.sobject_types[i->id];
        int const kFrame = i->cur_frame + t.start_frame;
        BlitShadowImage(kShadow, renderer.bmp, common.large_sprites.SpritePtr(kFrame),
                        i->x + kOffs.x - 3,
                        i->y + kOffs.y + 3,  // TODO: Original doesn't offset the shadow, which is
                                             // clearly wrong. Check that this offset is correct.
                        16, 16);
      }
    }

    {
      auto wr = game.wobjects.All();
      for (WObject const* i = nullptr; (i = wr.Next());) {
        Weapon const& w = *i->type;
        if (w.start_frame > -1) {
          int cur_frame = i->cur_frame;
          int const kShotType = w.shot_type;
          if (kShotType == 2) {
            cur_frame += 4;
            cur_frame >>= 3;
            if (cur_frame < 0) {
              cur_frame = 16;
            } else if (cur_frame > 15) {
              cur_frame -= 16;
            }
          } else if (kShotType == 3) {
            if (cur_frame > 64) {
              --cur_frame;
            }
            cur_frame -= 12;
            cur_frame >>= 3;
            if (cur_frame < 0) {
              cur_frame = 0;
            } else if (cur_frame > 12) {
              cur_frame = 12;
            }
          }
          int const kPosX = Ftoi(i->pos.x) - 3;
          int const kPosY = Ftoi(i->pos.y) - 3;
          if (w.shadow) {
            BlitShadowImage(kShadow, renderer.bmp,
                            common.small_sprites.SpritePtr(w.start_frame + cur_frame),
                            kPosX - 3 + kOffs.x, kPosY + 3 + kOffs.y, 7, 7);
          }
        } else if (i->cur_frame > 0) {
          int const kPosX = Ftoi(i->pos.x) + kOffs.x - 3;
          int const kPosY = Ftoi(i->pos.y) + kOffs.y + 3;
          uint32_t const kShadowed = kShadow.ShadowedArgb(kPosX, kPosY);
          if (kShadowed != 0 && renderer.bmp.clip_rect.Inside(kPosX, kPosY)) {
            renderer.bmp.GetPixel(kPosX, kPosY) = kShadowed;
          }
        }
      }
    }

    {
      auto nr = game.nobjects.All();
      for (NObject const* i = nullptr; (i = nr.Next());) {
        NObjectType const& t = *i->type;
        if (t.start_frame > 0) {
          auto pos = Ftoi(i->pos) - IVec2(3, 3);
          BlitShadowImage(kShadow, renderer.bmp,
                          common.small_sprites.SpritePtr(t.start_frame + i->cur_frame),
                          pos.x - 3 + kOffs.x, pos.y + 3 + kOffs.y, 7, 7);
        } else if (i->cur_frame > 1) {
          auto pos = Ftoi(i->pos) + kOffs;
          pos.x -= 3;
          pos.y += 3;
          if (renderer.bmp.clip_rect.Encloses(pos)) {
            uint32_t const kShadowed = kShadow.ShadowedArgb(pos.x, pos.y);
            if (kShadowed != 0) {
              renderer.bmp.GetPixel(pos.x, pos.y) = kShadowed;
            }
          }
        }
      }
    }

    for (auto const& worm_ptr : game.worms) {
      Worm const& w = *worm_ptr;
      if (w.visible) {
        int const kTempX = Ftoi(w.pos.x) - 7 + kOffs.x;
        int const kTempY = Ftoi(w.pos.y) - 5 + kOffs.y;
        if (w.ninjarope.out) {
          int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOffs.x;
          int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOffs.y;
          DrawShadowLine(kShadow, renderer.bmp, kNinjaropeX - 3, kNinjaropeY + 3, kTempX + 7 - 3,
                         kTempY + 4 + 3);
          BlitShadowImage(kShadow, renderer.bmp, common.large_sprites.SpritePtr(84),
                          kNinjaropeX - 4, kNinjaropeY + 2, 16, 16);
        }
        BlitShadowImage(kShadow, renderer.bmp,
                        common.WormSprite(w.current_frame, w.direction, w.index), kTempX - 3,
                        kTempY + 3, 16, 16);
      }
    }

    for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
      auto ipos = Ftoi(i->pos) + kOffs;
      ipos.x -= 3;
      ipos.y += 3;
      if (renderer.bmp.clip_rect.Encloses(ipos)) {
        uint32_t const kShadowed = kShadow.ShadowedArgb(ipos.x, ipos.y);
        if (kShadowed != 0) {
          renderer.bmp.GetPixel(ipos.x, ipos.y) = kShadowed;
        }
      }
    }
  }

  // Pass 2: all sprites, drawn on top of the fully-composited shadow layer.
  {
    auto br = game.bonuses.All();
    for (Bonus const* i = nullptr; (i = br.Next());) {
      if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
        int const kF = common.bonus_frames[i->frame];
        BlitImage(renderer.bmp, common.small_sprites[kF], Ftoi(i->x) - 3 + kOffs.x,
                  Ftoi(i->y) - 3 + kOffs.y);
        if (game.settings->names_on_bonuses && i->frame == 0) {
          std::string const& name = common.weapons[i->weapon].name;
          int const kLen = static_cast<int>(name.size()) * 4;
          common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(i->x) - kLen / 2 + kOffs.x,
                               Ftoi(i->y) - 10 + kOffs.y);
        }
      }
    }
  }

  {
    auto sr = game.sobjects.All();
    for (SObject const* i = nullptr; (i = sr.Next());) {
      SObjectType const& t = common.sobject_types[i->id];
      int const kFrame = i->cur_frame + t.start_frame;
      BlitImageR(kShadow, renderer.bmp, common.large_sprites.SpritePtr(kFrame), i->x + kOffs.x,
                 i->y + kOffs.y, 16, 16);
    }
  }

  {
    auto wr = game.wobjects.All();
    for (WObject* i = nullptr; (i = wr.Next());) {
      Weapon const& w = *i->type;
      if (w.start_frame > -1) {
        int cur_frame = i->cur_frame;
        int const kShotType = w.shot_type;
        if (kShotType == 2) {
          cur_frame += 4;
          cur_frame >>= 3;
          if (cur_frame < 0) {
            cur_frame = 16;
          } else if (cur_frame > 15) {
            cur_frame -= 16;
          }
        } else if (kShotType == 3) {
          if (cur_frame > 64) {
            --cur_frame;
          }
          cur_frame -= 12;
          cur_frame >>= 3;
          if (cur_frame < 0) {
            cur_frame = 0;
          } else if (cur_frame > 12) {
            cur_frame = 12;
          }
        }
        int const kPosX = Ftoi(i->pos.x) - 3;
        int const kPosY = Ftoi(i->pos.y) - 3;
        BlitImage(renderer.bmp, common.small_sprites[w.start_frame + cur_frame], kPosX + kOffs.x,
                  kPosY + kOffs.y);
      } else if (i->cur_frame > 0) {
        int const kPosX = Ftoi(i->pos.x) + kOffs.x;
        int const kPosY = Ftoi(i->pos.y) + kOffs.y;
        renderer.bmp.SetPixel(kPosX, kPosY, static_cast<PalIdx>(i->cur_frame));
      }

      if (!common.h[HRemExp] && i->type - common.weapons.data() == 34 &&
          game.settings->names_on_bonuses)  // TODO: Read from EXE
      {
        if (i->cur_frame == 0) {
          int const kNameNum =
              static_cast<int>(&*i - game.wobjects.arr) %
              static_cast<int>(common.weapons.size());  // TODO: Something nicer maybe

          std::string const& name = common.weapons[kNameNum].name;
          int const kWidth = static_cast<int>(name.size()) * 4;

          common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(i->pos.x) - kWidth / 2 + kOffs.x,
                               Ftoi(i->pos.y) - 10 + kOffs.y);
        }
      }
    }
  }

  {
    auto nr = game.nobjects.All();
    for (NObject const* i = nullptr; (i = nr.Next());) {
      NObjectType const& t = *i->type;
      if (t.start_frame > 0) {
        auto pos = Ftoi(i->pos) - IVec2(3, 3);
        BlitImage(renderer.bmp, common.small_sprites[t.start_frame + i->cur_frame], pos.x + kOffs.x,
                  pos.y + kOffs.y);
      } else if (i->cur_frame > 1) {
        auto pos = Ftoi(i->pos) + kOffs;
        if (renderer.bmp.clip_rect.Encloses(pos)) {
          renderer.bmp.SetPixel(pos.x, pos.y, static_cast<PalIdx>(i->cur_frame));
        }
      }
    }
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& w = *game.worms[i];

    if (w.visible) {
      int const kTempX = Ftoi(w.pos.x) - 7 + kOffs.x;
      int const kTempY = Ftoi(w.pos.y) - 5 + kOffs.y;
      int const kAngleFrame = w.AngleFrame();

      if (w.weapons[w.current_weapon].Available()) {
        int const kHotspotX = w.hotspot_x + kOffs.x;
        int const kHotspotY = w.hotspot_y + kOffs.y;

        WormWeapon const& ww = w.weapons[w.current_weapon];
        Weapon const& weapon = *ww.type;

        if (weapon.laser_sight) {
          DrawLaserSight(renderer.bmp, rand, kHotspotX, kHotspotY, kTempX + 7, kTempY + 4);
        }

        if (ww.type - common.weapons.data() == LC(LaserWeapon) - 1 && w.Pressed(Worm::kFire)) {
          DrawLine(renderer.bmp, kHotspotX, kHotspotY, kTempX + 7, kTempY + 4,
                   weapon.color_bullets);
        }
      }

      if (w.ninjarope.out) {
        int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOffs.x;
        int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOffs.y;

        DrawNinjarope(common, renderer.bmp, kNinjaropeX, kNinjaropeY, kTempX + 7, kTempY + 4);

        BlitImage(renderer.bmp, common.large_sprites[84], kNinjaropeX - 1, kNinjaropeY - 1);
      }

      if (w.weapons[w.current_weapon].type->fire_cone > 0 && w.fire_cone > 0) {
        /* TODO
        //NOTE! Check fctab so it's correct
        //NOTE! Check function 1071C and see what it actually does*/

        BlitFireCone(renderer.bmp, w.fire_cone / 2, common.FireConeSprite(kAngleFrame, w.direction),
                     Common::fire_cone_offset[w.direction][kAngleFrame][0] + kTempX,
                     Common::fire_cone_offset[w.direction][kAngleFrame][1] + kTempY);
      }

      BlitImage(renderer.bmp, common.WormSpriteObj(w.current_frame, w.direction, w.index), kTempX,
                kTempY);
    }

    if (w.ai) {
      w.ai->DrawDebug(game, w, renderer, kOffs.x, kOffs.y);
    }
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

  for (auto& i : game.worms) {
    Worm const& worm = *i;
    if (worm.visible) {
      auto temp =
          Ftoi(worm.pos) - IVec2(1, 2) + Ftoi(cossin_table[Ftoi(worm.aiming_angle)] * 16) + kOffs;
      // int tempX = ftoi(worm.pos.x) - 1 + ftoi(cosTable[ftoi(worm.aimingAngle)] * 16) + offs.x;
      // int tempY = ftoi(worm.pos.y) - 2 + ftoi(sinTable[ftoi(worm.aimingAngle)] * 16) + offs.y;

      BlitImage(renderer.bmp, common.small_sprites[worm.make_sight_green ? 44 : 43], temp.x,
                temp.y);

      if (worm.Pressed(Worm::kChange)) {
        std::string const& name = worm.weapons[worm.current_weapon].type->name;

        int const kLen = static_cast<int>(name.size()) * 4;  // TODO: Read 4 from exe? (SW_CHARWID)

        common.DrawTextSmall(renderer.bmp, name.c_str(), Ftoi(worm.pos.x) - kLen / 2 + 1 + kOffs.x,
                             Ftoi(worm.pos.y) - 10 + kOffs.y);
      }
    }
  }

  for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
    auto ipos = Ftoi(i->pos) + kOffs;
    if (renderer.bmp.clip_rect.Encloses(ipos)) {
      renderer.bmp.SetPixel(ipos.x, ipos.y, static_cast<PalIdx>(i->color));
    }
  }
}
