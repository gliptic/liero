#include "spectatorviewport.hpp"

#include <algorithm>
#include <cmath>
#include "constants.hpp"
#include "game.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/blit.hpp"
#include "gfx/renderer.hpp"
#include "gfx/shadow_query.hpp"
#include "math.hpp"
#include "text.hpp"

void SpectatorViewport::Process(Game& game) {
  int const kRenderW = render_w;
  int const kRenderH = render_h;

  // Bounding box of all worms + margin to keep both worms visible with headroom.
  int const kMargin = 60;
  int wmin_x = game.level.width;
  int wmin_y = game.level.height;
  int wmax_x = 0;
  int wmax_y = 0;
  for (auto const& worm_ptr : game.worms) {
    int const kWx = Ftoi(worm_ptr->pos.x);
    int const kWy = Ftoi(worm_ptr->pos.y);
    wmin_x = std::min(wmin_x, kWx);
    wmin_y = std::min(wmin_y, kWy);
    wmax_x = std::max(wmax_x, kWx);
    wmax_y = std::max(wmax_y, kWy);
  }

  int const kBboxW = std::max(1, wmax_x - wmin_x + 2 * kMargin);
  int const kBboxH = std::max(1, wmax_y - wmin_y + 2 * kMargin);
  float const kZoomX = static_cast<float>(kRenderW) / static_cast<float>(kBboxW);
  float const kZoomY = static_cast<float>(kRenderH) / static_cast<float>(kBboxH);
  // Cap at the zoom that shows the entire level — can be >1 for maps smaller
  // than the spectator window so the level fills the available space.
  float const kLevelFillZoom =
      std::min(static_cast<float>(kRenderW) / static_cast<float>(game.level.width),
               static_cast<float>(kRenderH) / static_cast<float>(game.level.height));
  zoom = std::min({kZoomX, kZoomY, kLevelFillZoom});

  int const kVisibleW = static_cast<int>(static_cast<float>(kRenderW) / zoom);
  int const kVisibleH = static_cast<int>(static_cast<float>(kRenderH) / zoom);

  // Center viewport on worm bounding box midpoint.
  x = (wmin_x + wmax_x) / 2 - kVisibleW / 2;
  y = (wmin_y + wmax_y) / 2 - kVisibleH / 2;

  // Screen shake (display-only, applied after centering).
  int const kRealShake = Ftoi(shake);
  if (kRealShake > 0) {
    x += rand(kRealShake * 2) - kRealShake;
    y += rand(kRealShake * 2) - kRealShake;
  }

  // Clamp to map bounds (max_x/y >= 0 even when visible region > map).
  max_x = std::max(0, game.level.width - kVisibleW);
  max_y = std::max(0, game.level.height - kVisibleH);
  x = std::clamp(x, 0, max_x);
  y = std::clamp(y, 0, max_y);

  // FIXME this is a bit broken
  if (game.WormByIdx(0)->killed_timer == Worm::kKilledTimerInitial ||
      game.WormByIdx(1)->killed_timer == Worm::kKilledTimerInitial) {
    banner_y = -8;
  }
}

void SpectatorViewport::Draw(Game& game, Renderer& renderer, GameState state, bool /*is_replay*/) {
  Common& common = *game.common;
  render_w = renderer.render_res_x;
  render_h = renderer.render_res_y;
  int const kMultiplier = render_w / 320;
  int const kCenterX = render_w / 2;

  // ── World pass ────────────────────────────────────────────────────────────
  // Scratch bitmap sized to the visible world region, capped to map dimensions.
  int const kScrW =
      std::min(static_cast<int>(static_cast<float>(render_w) / zoom), game.level.width);
  int const kScrH =
      std::min(static_cast<int>(static_cast<float>(render_h) / zoom), game.level.height);

  scratch_bmp.Alloc(kScrW, kScrH);
  scratch_bmp.pal32 = renderer.pal32;
  scratch_bmp.mode = renderer.mode;
  scratch_bmp.cycles = game.cycles;
  Fill(scratch_bmp, 0);

  // Offset: world pixel (x, y) maps to scratch pixel (0, 0).
  int const kOx = -x;
  int const kOy = -y;

  ShadowQuery const kShadow{.common = common,
                            .level = game.level,
                            .pal32 = renderer.pal32,
                            .world_offset_x = x,
                            .world_offset_y = y,
                            .mode = renderer.mode,
                            .cycles = game.cycles};

  DrawLevel(scratch_bmp, game.level, kOx, kOy);

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
    DrawDashedLineBox(scratch_bmp, game.holdazone.rect.x1 + kOx, game.holdazone.rect.y1 + kOy,
                      kColor, contender_color, game.holdazone.contender_frames,
                      Settings::kZoneCaptureTime, game.holdazone.rect.Width(),
                      game.holdazone.rect.Height(), game.cycles / 10);
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    if (!worm.visible && worm.killed_timer <= 0 && !worm.ready) {
      if (game.settings->allow_viewing_spawn_point && worm.Pressed(Worm::kChange)) {
        int const kTempX = Ftoi(worm.pos.x) - 7 + kOx;
        int const kTempY = Ftoi(worm.pos.y) - 5 + kOy;
        BlitImageTrans(scratch_bmp,
                       common.WormSpriteObj(worm.current_frame, worm.direction, worm.index), kTempX,
                       kTempY, game.cycles);
      }
    }
  }

  // Pass 1: all shadows before any sprite.
  if (game.settings->shadow) {
    {
      auto br = game.bonuses.All();
      for (Bonus const* i = nullptr; (i = br.Next());) {
        if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
          int const kF = common.bonus_frames[i->frame];
          BlitShadowImage(kShadow, scratch_bmp, common.small_sprites.SpritePtr(kF),
                          Ftoi(i->x) - 5 + kOx, Ftoi(i->y) - 1 + kOy, 7, 7);
        }
      }
    }

    {
      auto sr = game.sobjects.All();
      for (SObject const* i = nullptr; (i = sr.Next());) {
        SObjectType const& t = common.sobject_types[i->id];
        int const kFrame = i->cur_frame + t.start_frame;
        BlitShadowImage(kShadow, scratch_bmp, common.large_sprites.SpritePtr(kFrame),
                        i->x + kOx - 3, i->y + kOy + 3, 16, 16);
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
            BlitShadowImage(kShadow, scratch_bmp,
                            common.small_sprites.SpritePtr(w.start_frame + cur_frame),
                            kPosX - 3 + kOx, kPosY + 3 + kOy, 7, 7);
          }
        } else if (i->cur_frame > 0) {
          int const kPosX = Ftoi(i->pos.x) + kOx - 3;
          int const kPosY = Ftoi(i->pos.y) + kOy + 3;
          uint32_t const kShadowed = kShadow.ShadowedArgb(kPosX, kPosY);
          if (kShadowed != 0 && scratch_bmp.clip_rect.Inside(kPosX, kPosY)) {
            scratch_bmp.GetPixel(kPosX, kPosY) = kShadowed;
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
          BlitShadowImage(kShadow, scratch_bmp,
                          common.small_sprites.SpritePtr(t.start_frame + i->cur_frame),
                          pos.x - 3 + kOx, pos.y + 3 + kOy, 7, 7);
        } else if (i->cur_frame > 1) {
          auto pos = Ftoi(i->pos);
          pos.x += kOx - 3;
          pos.y += kOy + 3;
          if (scratch_bmp.clip_rect.Encloses(pos)) {
            uint32_t const kShadowed = kShadow.ShadowedArgb(pos.x, pos.y);
            if (kShadowed != 0) {
              scratch_bmp.GetPixel(pos.x, pos.y) = kShadowed;
            }
          }
        }
      }
    }

    for (auto const& worm_ptr : game.worms) {
      Worm const& w = *worm_ptr;
      if (w.visible) {
        int const kTempX = Ftoi(w.pos.x) - 7 + kOx;
        int const kTempY = Ftoi(w.pos.y) - 5 + kOy;
        if (w.ninjarope.out) {
          int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOx;
          int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOy;
          DrawShadowLine(kShadow, scratch_bmp, kNinjaropeX - 3, kNinjaropeY + 3, kTempX + 7 - 3,
                         kTempY + 4 + 3);
          BlitShadowImage(kShadow, scratch_bmp, common.large_sprites.SpritePtr(84), kNinjaropeX - 4,
                          kNinjaropeY + 2, 16, 16);
        }
        BlitShadowImage(kShadow, scratch_bmp,
                        common.WormSprite(w.current_frame, w.direction, w.index), kTempX - 3,
                        kTempY + 3, 16, 16);
      }
    }

    for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
      auto ipos = Ftoi(i->pos);
      ipos.x += kOx - 3;
      ipos.y += kOy + 3;
      if (scratch_bmp.clip_rect.Encloses(ipos)) {
        uint32_t const kShadowed = kShadow.ShadowedArgb(ipos.x, ipos.y);
        if (kShadowed != 0) {
          scratch_bmp.GetPixel(ipos.x, ipos.y) = kShadowed;
        }
      }
    }
  }

  // Pass 2: all sprites on top of the fully-composited shadow layer.
  {
    auto br = game.bonuses.All();
    for (Bonus const* i = nullptr; (i = br.Next());) {
      if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
        int const kF = common.bonus_frames[i->frame];
        BlitImage(scratch_bmp, common.small_sprites[kF], Ftoi(i->x) - 3 + kOx,
                  Ftoi(i->y) - 3 + kOy);
        if (game.settings->names_on_bonuses && i->frame == 0) {
          std::string const& name = common.weapons[i->weapon].name;
          int const kLen = static_cast<int>(name.size()) * 4;
          common.DrawTextSmall(scratch_bmp, name.c_str(), Ftoi(i->x) - kLen / 2 + kOx,
                               Ftoi(i->y) - 10 + kOy);
        }
      }
    }
  }

  {
    auto sr = game.sobjects.All();
    for (SObject const* i = nullptr; (i = sr.Next());) {
      SObjectType const& t = common.sobject_types[i->id];
      int const kFrame = i->cur_frame + t.start_frame;
      BlitImageR(kShadow, scratch_bmp, common.large_sprites.SpritePtr(kFrame), i->x + kOx,
                 i->y + kOy, 16, 16);
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
        BlitImage(scratch_bmp, common.small_sprites[w.start_frame + cur_frame], kPosX + kOx,
                  kPosY + kOy);
      } else if (i->cur_frame > 0) {
        int const kPosX = Ftoi(i->pos.x) + kOx;
        int const kPosY = Ftoi(i->pos.y) + kOy;
        scratch_bmp.SetPixel(kPosX, kPosY, static_cast<PalIdx>(i->cur_frame));
      }
      if (!common.h[HRemExp] && i->type - common.weapons.data() == 34 &&
          game.settings->names_on_bonuses) {
        if (i->cur_frame == 0) {
          int const kNameNum =
              static_cast<int>(&*i - game.wobjects.arr) % static_cast<int>(common.weapons.size());
          std::string const& name = common.weapons[kNameNum].name;
          int const kWidth = static_cast<int>(name.size()) * 4;
          common.DrawTextSmall(scratch_bmp, name.c_str(), Ftoi(i->pos.x) - kWidth / 2 + kOx,
                               Ftoi(i->pos.y) - 10 + kOy);
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
        BlitImage(scratch_bmp, common.small_sprites[t.start_frame + i->cur_frame], pos.x + kOx,
                  pos.y + kOy);
      } else if (i->cur_frame > 1) {
        auto pos = Ftoi(i->pos);
        pos.x += kOx;
        pos.y += kOy;
        if (scratch_bmp.clip_rect.Encloses(pos)) {
          scratch_bmp.SetPixel(pos.x, pos.y, static_cast<PalIdx>(i->cur_frame));
        }
      }
    }
  }

  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& w = *game.worms[i];
    if (w.visible) {
      int const kTempX = Ftoi(w.pos.x) - 7 + kOx;
      int const kTempY = Ftoi(w.pos.y) - 5 + kOy;
      int const kAngleFrame = w.AngleFrame();
      if (w.weapons[w.current_weapon].Available()) {
        int const kHotspotX = w.hotspot_x + kOx;
        int const kHotspotY = w.hotspot_y + kOy;
        WormWeapon const& ww = w.weapons[w.current_weapon];
        Weapon const& weapon = *ww.type;
        if (weapon.laser_sight) {
          DrawLaserSight(scratch_bmp, rand, kHotspotX, kHotspotY, kTempX + 7, kTempY + 4);
        }
        if (ww.type - common.weapons.data() == LC(LaserWeapon) - 1 && w.Pressed(Worm::kFire)) {
          DrawLine(scratch_bmp, kHotspotX, kHotspotY, kTempX + 7, kTempY + 4, weapon.color_bullets);
        }
      }
      if (w.ninjarope.out) {
        int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOx;
        int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOy;
        DrawNinjarope(common, scratch_bmp, kNinjaropeX, kNinjaropeY, kTempX + 7, kTempY + 4);
        BlitImage(scratch_bmp, common.large_sprites[84], kNinjaropeX - 1, kNinjaropeY - 1);
      }
      if (w.weapons[w.current_weapon].type->fire_cone > 0 && w.fire_cone > 0) {
        BlitFireCone(scratch_bmp, w.fire_cone / 2, common.FireConeSprite(kAngleFrame, w.direction),
                     Common::fire_cone_offset[w.direction][kAngleFrame][0] + kTempX,
                     Common::fire_cone_offset[w.direction][kAngleFrame][1] + kTempY);
      }
      BlitImage(scratch_bmp, common.WormSpriteObj(w.current_frame, w.direction, w.index), kTempX,
                kTempY);
    }
    if (w.ai) {
      w.ai->DrawDebug(game, w, renderer, kOx, kOy);
    }
  }

  for (auto& i : game.worms) {
    Worm const& worm = *i;
    if (worm.visible) {
      auto temp = Ftoi(worm.pos) - IVec2(1, 2) + Ftoi(cossin_table[Ftoi(worm.aiming_angle)] * 16);
      temp.x += kOx;
      temp.y += kOy;
      BlitImage(scratch_bmp, common.small_sprites[worm.make_sight_green ? 44 : 43], temp.x, temp.y);
      if (worm.Pressed(Worm::kChange)) {
        std::string const& name = worm.weapons[worm.current_weapon].type->name;
        int const kLen = static_cast<int>(name.size()) * 4;
        common.DrawTextSmall(scratch_bmp, name.c_str(), Ftoi(worm.pos.x) - kLen / 2 + 1 + kOx,
                             Ftoi(worm.pos.y) - 10 + kOy);
      }
    }
  }

  for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
    auto ipos = Ftoi(i->pos);
    ipos.x += kOx;
    ipos.y += kOy;
    if (scratch_bmp.clip_rect.Encloses(ipos)) {
      scratch_bmp.SetPixel(ipos.x, ipos.y, static_cast<PalIdx>(i->color));
    }
  }

  // ── Composite scratch → renderer ─────────────────────────────────────────
  // Scale the scratch into a centred output rect that preserves the level's
  // aspect ratio; any remaining bars are filled black.
  int const kOutW =
      std::min(static_cast<int>(std::lroundf(static_cast<float>(kScrW) * zoom)), render_w);
  int const kOutH =
      std::min(static_cast<int>(std::lroundf(static_cast<float>(kScrH) * zoom)), render_h);
  int const kOutX = (render_w - kOutW) / 2;
  int const kOutY = (render_h - kOutH) / 2;

  if (kOutX > 0 || kOutY > 0) {
    Fill(renderer.bmp, 0);
  }

  if (kScrW == kOutW && kScrH == kOutH) {
    BlitBitmap(renderer.bmp, scratch_bmp, kOutX, kOutY, kScrW, kScrH);
  } else {
    uint32_t* const kDest = renderer.bmp.pixels +
                            static_cast<std::size_t>(kOutY) * renderer.bmp.pitch +
                            static_cast<std::size_t>(kOutX);
    ScaleDrawArea(scratch_bmp.pixels, kScrW, kScrH, scratch_bmp.pitch, kDest, kOutW, kOutH,
                  renderer.bmp.pitch);
  }

  // ── HUD overlay (native resolution, drawn on top) ─────────────────────────
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
    int const kHudX = worm.stats_x * kMultiplier;
    int const kOffsetWeaponListX = kCenterX - 15 + (i == 0 ? -90 : 60);

    if (worm.visible) {
      int const kLifebarWidth = worm.health * 100 / worm.settings->health;
      DrawBar(renderer.bmp, kHudX, renderer.render_res_y - 39, kLifebarWidth,
              kLifebarWidth / 10 + 234);
    } else {
      int lifebar_width = 100 - (worm.killed_timer * 25) / 37;
      if (lifebar_width > 0) {
        lifebar_width = std::min(lifebar_width, 100);
        DrawBar(renderer.bmp, kHudX, renderer.render_res_y - 39, lifebar_width,
                lifebar_width / 10 + 234);
      }
    }

    WormWeapon const& ww = worm.weapons[worm.current_weapon];
    if (ww.Available()) {
      if (ww.ammo > 0) {
        int const kAmmoBarWidth = ww.ammo * 100 / ww.type->ammo;
        if (kAmmoBarWidth > 0) {
          DrawBar(renderer.bmp, kHudX, renderer.render_res_y - 34, kAmmoBarWidth,
                  kAmmoBarWidth / 10 + 245);
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
        DrawBar(renderer.bmp, kHudX, renderer.render_res_y - 34, ammo_bar_width,
                ammo_bar_width / 10 + 245);
      }
      if ((game.cycles % 20) > 10 && worm.visible) {
        common.font.DrawString(renderer.bmp, LS(Reloading), kHudX, 164, 50);
      }
    }

    common.font.DrawString(renderer.bmp, (LS(Kills) + ToString(worm.kills)), kHudX,
                           renderer.render_res_y - 29, 10);
    common.font.DrawString(renderer.bmp, worm.settings->name, kHudX, renderer.render_res_y - 15, 7);
    FillRect(renderer.bmp, kHudX - 1, renderer.render_res_y - 7 - 1, 8, 8, 7);
    FillRect(renderer.bmp, kHudX, renderer.render_res_y - 7, 6, 6, worm.settings->color);
    // FIXME: only draw this once, not once per worm
    common.font.DrawString(
        renderer.bmp,
        TimeToStringEx(game.cycles * 14, /*force_hours=*/false, /*force_minutes=*/true),
        kCenterX - 15, renderer.render_res_y - 15, 7);

    if (state == kStateGame) {
      for (int j = 0; j < 5; j++) {
        WormWeapon const& wwj = worm.weapons[j];
        if (wwj.ammo > 0) {
          int const kAmmoBarWidth = wwj.ammo * 60 / wwj.type->ammo;
          if (kAmmoBarWidth > 0) {
            DrawBar(renderer.bmp, kOffsetWeaponListX, renderer.render_res_y - 40 + j * 8,
                    kAmmoBarWidth, 5, kAmmoBarWidth / 6 + 245);
          }
        }
        if (worm.current_weapon == j) {
          common.font.DrawString(renderer.bmp, worm.weapons[j].type->name, kOffsetWeaponListX,
                                 renderer.render_res_y - 40 + j * 8, 187);
        } else {
          common.font.DrawString(renderer.bmp, worm.weapons[j].type->name, kOffsetWeaponListX,
                                 renderer.render_res_y - 40 + j * 8, 185);
        }
      }
    }

    int const kStateColours[2][2] = {{6, 10}, {79, 4}};
    switch (game.settings->game_mode) {
      case Settings::kGmKillEmAll:
      case Settings::kGmScalesOfJustice: {
        common.font.DrawString(renderer.bmp, (LS(Lives) + ToString(worm.lives)), kHudX,
                               renderer.render_res_y - 22, 6);
      } break;
      case Settings::kGmHoldazone: {
        int hstate = 0;
        for (auto const& w : game.worms) {
          if (w.get() != &worm && w->timer <= worm.timer) {
            hstate = 1;
          }
        }
        int const kColor = kStateColours[game.holdazone.holder_idx != worm.index][hstate];
        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, kColor);
      } break;
      case Settings::kGmGameOfTag: {
        int gstate = 0;
        for (auto const& w : game.worms) {
          if (w.get() != &worm && w->timer >= worm.timer) {
            gstate = 1;
          }
        }
        int const kColor = kStateColours[game.last_killed_idx != worm.index][gstate];
        common.font.DrawString(renderer.bmp, TimeToString(worm.timer), 5, 106 + 84 * worm.index,
                               renderer.render_res_y - 39, kColor);
      } break;
      default:
        break;
    }
  }

  // ── Banner messages ───────────────────────────────────────────────────────
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm const& worm = *game.worms[i];
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
}
