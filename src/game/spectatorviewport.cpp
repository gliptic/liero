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
#include "profiling.hpp"
#include "text.hpp"

float ComputeSpectatorZoom(int render_w, int render_h, int bbox_w, int bbox_h, int level_w,
                           int level_h) {
  float const kZoomX = static_cast<float>(render_w) / static_cast<float>(bbox_w);
  float const kZoomY = static_cast<float>(render_h) / static_cast<float>(bbox_h);
  // Zoom that shows the entire level — can be >1 for maps smaller than the
  // spectator window so the level fills the available space.
  float const kLevelFillZoom = std::min(static_cast<float>(render_w) / static_cast<float>(level_w),
                                        static_cast<float>(render_h) / static_cast<float>(level_h));
  // Never zoom out past the whole-level fit (kLevelFillZoom floor); never
  // upscale past native unless the level is smaller than the window
  // (kLevelFillZoom ceiling). The floor is what keeps a level that already
  // fits from shrinking when worms reach the edges.
  float const kMaxZoom = std::max(1.0F, kLevelFillZoom);
  return std::clamp(std::min(kZoomX, kZoomY), kLevelFillZoom, kMaxZoom);
}

SpectatorDstRect ComputeSpectatorDstRect(int render_w, int render_h, int scr_w, int scr_h,
                                         float zoom) {
  int const kOutW =
      std::min(static_cast<int>(std::lroundf(static_cast<float>(scr_w) * zoom)), render_w);
  int const kOutH =
      std::min(static_cast<int>(std::lroundf(static_cast<float>(scr_h) * zoom)), render_h);
  return {.x = (render_w - kOutW) / 2, .y = (render_h - kOutH) / 2, .w = kOutW, .h = kOutH};
}

WorldPassScratch ComputeWorldPassScratch(int render_w, int render_h, float zoom, int level_w,
                                         int level_h) {
  float const kScale = std::min(1.0F, zoom);
  // Visible world region at 1:1 (same as the legacy scratch size).
  int const kViewW = std::min(static_cast<int>(static_cast<float>(render_w) / zoom), level_w);
  int const kViewH = std::min(static_cast<int>(static_cast<float>(render_h) / zoom), level_h);
  int const kW = std::max(1, static_cast<int>(std::lroundf(static_cast<float>(kViewW) * kScale)));
  int const kH = std::max(1, static_cast<int>(std::lroundf(static_cast<float>(kViewH) * kScale)));
  return {.w = kW, .h = kH, .scale = kScale};
}

CappedRenderResolution ComputeCappedRenderResolution(int window_w, int window_h, int cap_h) {
  // Disabled, or the window already fits under the cap → render at native res
  // (never upscale). Small-window output stays byte-for-byte unchanged.
  if (cap_h <= 0 || window_h <= cap_h) {
    return {.w = window_w, .h = window_h};
  }
  // Pin height to the cap, derive width from the window aspect so the spectator
  // overview keeps its shape (ultrawide stays ultrawide). lroundf for symmetric
  // rounding; guard against a sub-pixel width on extreme aspect ratios.
  float const kScale = static_cast<float>(cap_h) / static_cast<float>(window_h);
  int const kW = std::max(1, static_cast<int>(std::lroundf(static_cast<float>(window_w) * kScale)));
  return {.w = kW, .h = cap_h};
}

HudDirtyBands ComputeHudDirtyBands(int render_h, int banner_y, int prev_banner_y) {
  // Glyph height (Font::DrawChar draws 8 rows at size 1); the banner adds a 1px
  // drop shadow. The bottom stats strip spans the lowest 40px (weapon list,
  // life/ammo bars, kills/name/time text, colour box). y=164 is the fixed
  // "Reloading" text row. These match the draws in SpectatorViewport::Draw's
  // HUD block — keep them in sync if those move.
  constexpr int kTextH = 8;
  constexpr int kBottomStripH = 40;
  constexpr int kReloadingY = 164;
  constexpr int kBannerHiddenY = -8;  // banner_y == -8 means hidden (not drawn)

  HudDirtyBands out{};

  // Append [y, y+h) clamped to [0, render_h); drop if it ends up empty.
  auto push = [&](int y, int h) {
    int const kY0 = std::max(0, y);
    int const kY1 = std::min(render_h, y + h);
    if (kY1 > kY0 && out.count < HudDirtyBands::kMaxBands) {
      out.bands[out.count++] = {.y = kY0, .h = kY1 - kY0};
    }
  };

  push(render_h - kBottomStripH, kBottomStripH);
  push(kReloadingY, kTextH);

  // Banner band spans both frames' extents so a scrolling banner clears the row
  // it vacated. Skipped only when hidden in both frames (nothing was drawn).
  int const kLo = std::min(banner_y, prev_banner_y);
  int const kHi = std::max(banner_y, prev_banner_y);
  if (kHi > kBannerHiddenY) {
    push(kLo, (kHi + kTextH + 1) - kLo);
  }

  return out;
}

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
  zoom =
      ComputeSpectatorZoom(kRenderW, kRenderH, kBboxW, kBboxH, game.level.width, game.level.height);

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
  ZoneScopedN("SpectatorViewport::Draw");
  Common& common = *game.common;
  render_w = renderer.render_res_x;
  render_h = renderer.render_res_y;
  int const kCenterX = render_w / 2;

  // ── World pass ────────────────────────────────────────────────────────────
  // kViewW/kViewH is the visible world region at 1:1. When the level fits
  // (zoom >= 1) the scratch is that region and the world is drawn 1:1 (the GPU
  // upscales it). When zoomed out (zoom < 1) the world is instead rendered at
  // ~output resolution into a smaller scratch, so the world pass and its
  // texture upload are bounded by the window, not the level area.
  int const kViewW =
      std::min(static_cast<int>(static_cast<float>(render_w) / zoom), game.level.width);
  int const kViewH =
      std::min(static_cast<int>(static_cast<float>(render_h) / zoom), game.level.height);
  WorldPassScratch const kWp =
      ComputeWorldPassScratch(render_w, render_h, zoom, game.level.width, game.level.height);

  scratch_bmp.Alloc(kWp.w, kWp.h);
  scratch_bmp.pal32 = renderer.pal32;
  scratch_bmp.mode = renderer.mode;
  scratch_bmp.cycles = game.cycles;
  Fill(scratch_bmp, 0);

  if (kWp.scale < 1.0F) {
    // ── Downscaled overview (zoom < 1) ──────────────────────────────────────
    // Render the visually-significant world at ~output resolution. Detail that
    // is sub-pixel or illegible at this zoom — shadows, text labels, fire
    // cones, laser sights, the aim crosshair, AI debug — is intentionally
    // omitted; it cannot be seen and is what made the 1:1 path expensive.
    ZoneScopedN("Spectator::WorldPass::Downscaled");
    float const kScale = kWp.scale;
    auto sx = [&](int wx) {
      return static_cast<int>(std::lroundf(static_cast<float>(wx - x) * kScale));
    };
    auto sy = [&](int wy) {
      return static_cast<int>(std::lroundf(static_cast<float>(wy - y) * kScale));
    };
    // World-space AABB cull against the visible region [x, x+kViewW) x [y, y+kViewH).
    auto in_view = [&](int wx, int wy, int sz) {
      return wx + sz >= x && wx < x + kViewW && wy + sz >= y && wy < y + kViewH;
    };

    {
      ZoneScopedN("Spectator::WorldPass::DrawLevel");
      DrawLevelScaled(scratch_bmp, game.level, x, y, kScale);
    }

    {
      auto br = game.bonuses.All();
      for (Bonus const* i = nullptr; (i = br.Next());) {
        int const kWx = Ftoi(i->x) - 3;
        int const kWy = Ftoi(i->y) - 3;
        if (!in_view(kWx, kWy, 7)) {
          continue;
        }
        if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
          BlitImageScaled(scratch_bmp, common.small_sprites[common.bonus_frames[i->frame]], sx(kWx),
                          sy(kWy), kScale);
        }
      }
    }

    {
      auto sr = game.sobjects.All();
      for (SObject const* i = nullptr; (i = sr.Next());) {
        if (!in_view(i->x, i->y, 16)) {
          continue;
        }
        SObjectType const& t = common.sobject_types[i->id];
        BlitImageScaled(scratch_bmp, common.large_sprites[i->cur_frame + t.start_frame], sx(i->x),
                        sy(i->y), kScale);
      }
    }

    {
      auto wr = game.wobjects.All();
      for (WObject const* i = nullptr; (i = wr.Next());) {
        int const kWx = Ftoi(i->pos.x) - 3;
        int const kWy = Ftoi(i->pos.y) - 3;
        if (!in_view(kWx, kWy, 7)) {
          continue;
        }
        Weapon const& w = *i->type;
        if (w.start_frame > -1) {
          int cur_frame = i->cur_frame;
          int const kShotType = w.shot_type;
          if (kShotType == 2) {
            cur_frame = (cur_frame + 4) >> 3;
            if (cur_frame < 0) {
              cur_frame = 16;
            } else if (cur_frame > 15) {
              cur_frame -= 16;
            }
          } else if (kShotType == 3) {
            if (cur_frame > 64) {
              --cur_frame;
            }
            cur_frame = (cur_frame - 12) >> 3;
            cur_frame = std::clamp(cur_frame, 0, 12);
          }
          BlitImageScaled(scratch_bmp, common.small_sprites[w.start_frame + cur_frame], sx(kWx),
                          sy(kWy), kScale);
        } else if (i->cur_frame > 0) {
          scratch_bmp.SetPixel(sx(Ftoi(i->pos.x)), sy(Ftoi(i->pos.y)),
                               static_cast<PalIdx>(i->cur_frame));
        }
      }
    }

    {
      auto nr = game.nobjects.All();
      for (NObject const* i = nullptr; (i = nr.Next());) {
        int const kWx = Ftoi(i->pos.x) - 3;
        int const kWy = Ftoi(i->pos.y) - 3;
        if (!in_view(kWx, kWy, 7)) {
          continue;
        }
        NObjectType const& t = *i->type;
        if (t.start_frame > 0) {
          BlitImageScaled(scratch_bmp, common.small_sprites[t.start_frame + i->cur_frame], sx(kWx),
                          sy(kWy), kScale);
        } else if (i->cur_frame > 1) {
          scratch_bmp.SetPixel(sx(Ftoi(i->pos.x)), sy(Ftoi(i->pos.y)),
                               static_cast<PalIdx>(i->cur_frame));
        }
      }
    }

    for (auto const& worm_ptr : game.worms) {
      Worm const& w = *worm_ptr;
      if (!w.visible) {
        continue;
      }
      int const kWx = Ftoi(w.pos.x) - 7;
      int const kWy = Ftoi(w.pos.y) - 5;
      if (!in_view(kWx, kWy, 16)) {
        continue;
      }
      if (w.ninjarope.out) {
        DrawNinjarope(common, scratch_bmp, sx(Ftoi(w.ninjarope.pos.x)), sy(Ftoi(w.ninjarope.pos.y)),
                      sx(Ftoi(w.pos.x)), sy(Ftoi(w.pos.y) - 1));
        BlitImageScaled(scratch_bmp, common.large_sprites[84], sx(Ftoi(w.ninjarope.pos.x) - 1),
                        sy(Ftoi(w.ninjarope.pos.y) - 1), kScale);
      }
      BlitImageScaled(scratch_bmp, common.WormSpriteObj(w.current_frame, w.direction, w.index),
                      sx(kWx), sy(kWy), kScale);
    }

    for (Game::BObjectList::Iterator i = game.bobjects.Begin(); i != game.bobjects.End(); ++i) {
      auto ipos = Ftoi(i->pos);
      scratch_bmp.SetPixel(sx(ipos.x), sy(ipos.y), static_cast<PalIdx>(i->color));
    }
  } else {
    int const kScrW = kWp.w;
    int const kScrH = kWp.h;
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

    {
      ZoneScopedN("Spectator::WorldPass::DrawLevel");
      DrawLevel(scratch_bmp, game.level, kOx, kOy);
    }

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
                         common.WormSpriteObj(worm.current_frame, worm.direction, worm.index),
                         kTempX, kTempY, game.cycles);
        }
      }
    }

    // Pass 1: all shadows before any sprite.
    if (game.settings->shadow) {
      ZoneScopedN("Spectator::WorldPass::ShadowPass");
      {
        auto br = game.bonuses.All();
        for (Bonus const* i = nullptr; (i = br.Next());) {
          int const kBx = Ftoi(i->x) - 3 + kOx;
          int const kBy = Ftoi(i->y) - 3 + kOy;
          if (kBx + 7 < 0 || kBx >= kScrW || kBy + 7 < 0 || kBy >= kScrH) {
            continue;
          }
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
          int const kSx = i->x + kOx;
          int const kSy = i->y + kOy;
          if (kSx + 16 < 0 || kSx >= kScrW || kSy + 16 < 0 || kSy >= kScrH) {
            continue;
          }
          SObjectType const& t = common.sobject_types[i->id];
          int const kFrame = i->cur_frame + t.start_frame;
          BlitShadowImage(kShadow, scratch_bmp, common.large_sprites.SpritePtr(kFrame),
                          i->x + kOx - 3, i->y + kOy + 3, 16, 16);
        }
      }

      {
        auto wr = game.wobjects.All();
        for (WObject const* i = nullptr; (i = wr.Next());) {
          int const kWx = Ftoi(i->pos.x) - 3 + kOx;
          int const kWy = Ftoi(i->pos.y) - 3 + kOy;
          if (kWx + 7 < 0 || kWx >= kScrW || kWy + 7 < 0 || kWy >= kScrH) {
            continue;
          }
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
          int const kNx = Ftoi(i->pos.x) - 3 + kOx;
          int const kNy = Ftoi(i->pos.y) - 3 + kOy;
          if (kNx + 7 < 0 || kNx >= kScrW || kNy + 7 < 0 || kNy >= kScrH) {
            continue;
          }
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
          if (kTempX + 16 < 0 || kTempX >= kScrW || kTempY + 16 < 0 || kTempY >= kScrH) {
            continue;
          }
          if (w.ninjarope.out) {
            int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOx;
            int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOy;
            DrawShadowLine(kShadow, scratch_bmp, kNinjaropeX - 3, kNinjaropeY + 3, kTempX + 7 - 3,
                           kTempY + 4 + 3);
            BlitShadowImage(kShadow, scratch_bmp, common.large_sprites.SpritePtr(84),
                            kNinjaropeX - 4, kNinjaropeY + 2, 16, 16);
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
      ZoneScopedN("Spectator::WorldPass::SpritePass");
      {
        auto br = game.bonuses.All();
        for (Bonus const* i = nullptr; (i = br.Next());) {
          int const kBx = Ftoi(i->x) - 3 + kOx;
          int const kBy = Ftoi(i->y) - 3 + kOy;
          if (kBx + 7 < 0 || kBx >= kScrW || kBy + 7 < 0 || kBy >= kScrH) {
            continue;
          }
          if (i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0) {
            int const kF = common.bonus_frames[i->frame];
            BlitImage(scratch_bmp, common.small_sprites[kF], kBx, kBy);
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
          int const kSx = i->x + kOx;
          int const kSy = i->y + kOy;
          if (kSx + 16 < 0 || kSx >= kScrW || kSy + 16 < 0 || kSy >= kScrH) {
            continue;
          }
          SObjectType const& t = common.sobject_types[i->id];
          int const kFrame = i->cur_frame + t.start_frame;
          BlitImageR(kShadow, scratch_bmp, common.large_sprites.SpritePtr(kFrame), kSx, kSy, 16,
                     16);
        }
      }

      {
        auto wr = game.wobjects.All();
        for (WObject* i = nullptr; (i = wr.Next());) {
          int const kWx = Ftoi(i->pos.x) - 3 + kOx;
          int const kWy = Ftoi(i->pos.y) - 3 + kOy;
          if (kWx + 7 < 0 || kWx >= kScrW || kWy + 7 < 0 || kWy >= kScrH) {
            continue;
          }
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
              int const kNameNum = static_cast<int>(&*i - game.wobjects.arr) %
                                   static_cast<int>(common.weapons.size());
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
          int const kNx = Ftoi(i->pos.x) - 3 + kOx;
          int const kNy = Ftoi(i->pos.y) - 3 + kOy;
          if (kNx + 7 < 0 || kNx >= kScrW || kNy + 7 < 0 || kNy >= kScrH) {
            continue;
          }
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
          if (kTempX + 16 < 0 || kTempX >= kScrW || kTempY + 16 < 0 || kTempY >= kScrH) {
            continue;
          }
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
              DrawLine(scratch_bmp, kHotspotX, kHotspotY, kTempX + 7, kTempY + 4,
                       weapon.color_bullets);
            }
          }
          if (w.ninjarope.out) {
            int const kNinjaropeX = Ftoi(w.ninjarope.pos.x) + kOx;
            int const kNinjaropeY = Ftoi(w.ninjarope.pos.y) + kOy;
            DrawNinjarope(common, scratch_bmp, kNinjaropeX, kNinjaropeY, kTempX + 7, kTempY + 4);
            BlitImage(scratch_bmp, common.large_sprites[84], kNinjaropeX - 1, kNinjaropeY - 1);
          }
          if (w.weapons[w.current_weapon].type->fire_cone > 0 && w.fire_cone > 0) {
            BlitFireCone(scratch_bmp, w.fire_cone / 2,
                         common.FireConeSprite(kAngleFrame, w.direction),
                         Common::fire_cone_offset[w.direction][kAngleFrame][0] + kTempX,
                         Common::fire_cone_offset[w.direction][kAngleFrame][1] + kTempY);
          }
          BlitImage(scratch_bmp, common.WormSpriteObj(w.current_frame, w.direction, w.index),
                    kTempX, kTempY);
        }
        if (w.ai) {
          w.ai->DrawDebug(game, w, renderer, kOx, kOy);
        }
      }

      for (auto& i : game.worms) {
        Worm const& worm = *i;
        if (worm.visible) {
          auto temp =
              Ftoi(worm.pos) - IVec2(1, 2) + Ftoi(cossin_table[Ftoi(worm.aiming_angle)] * 16);
          temp.x += kOx;
          temp.y += kOy;
          if (temp.x + 7 < 0 || temp.x >= kScrW || temp.y + 7 < 0 || temp.y >= kScrH) {
            continue;
          }
          BlitImage(scratch_bmp, common.small_sprites[worm.make_sight_green ? 44 : 43], temp.x,
                    temp.y);
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
    }  // end SpritePass zone
  }  // end 1:1 world pass

  // ── Composite scratch → renderer ─────────────────────────────────────────
  // Centred output rect that preserves the level's aspect ratio; any remaining
  // bars are filled black. The dst rect is computed from the visible region at
  // 1:1 (kViewW/kViewH) so it is independent of the scratch's render scale;
  // shared by the CPU and GPU composite paths.
  SpectatorDstRect const kDst = ComputeSpectatorDstRect(render_w, render_h, kViewW, kViewH, zoom);

  if (renderer.gpu_world_composite) {
    // GPU path: hand the world pass to Gfx, which uploads the used sub-rect
    // to a streaming texture and scales it on the GPU (SDL_RenderTexture).
    // `bmp` becomes a transparent HUD-only overlay blended on top of the world.
    // The scratch is bounded by the window, so the texture is sized to the
    // render surface rather than the level.
    ZoneScopedN("Spectator::Composite::GpuHandoff");
    renderer.gpu_world_src = &scratch_bmp;
    renderer.gpu_world_used_w = kWp.w;
    renderer.gpu_world_used_h = kWp.h;
    renderer.gpu_world_max_w = render_w;
    renderer.gpu_world_max_h = render_h;
    renderer.gpu_world_dst_x = kDst.x;
    renderer.gpu_world_dst_y = kDst.y;
    renderer.gpu_world_dst_w = kDst.w;
    renderer.gpu_world_dst_h = kDst.h;

    // Clear only the rows the HUD will draw into this frame plus the previous
    // frame's banner row, leaving the rest of the overlay untouched. Force a
    // full clear on the first frame / after a resolution change so untouched
    // regions start transparent. The same bands drive the partial texture
    // upload in Gfx::DrawSpectatorGpu.
    bool const kFullRefresh = (render_w != hud_overlay_w || render_h != hud_overlay_h);
    HudDirtyBands const kBands = ComputeHudDirtyBands(render_h, banner_y, prev_banner_y);
    if (kFullRefresh) {
      FillTransparent(renderer.bmp);
    } else {
      for (int i = 0; i < kBands.count; ++i) {
        FillTransparentBand(renderer.bmp, kBands.bands[i].y, kBands.bands[i].h);
      }
    }
    renderer.hud_overlay_full_refresh = kFullRefresh;
    renderer.hud_overlay_band_count = kBands.count;
    for (int i = 0; i < kBands.count; ++i) {
      renderer.hud_overlay_band_y[i] = kBands.bands[i].y;
      renderer.hud_overlay_band_h[i] = kBands.bands[i].h;
    }
  } else {
    // CPU path (videotool, single-screen replay, dummy driver): box-filter the
    // scratch straight into `bmp` via ScaleDrawArea.
    ZoneScopedN("Spectator::Composite");
    renderer.gpu_world_src = nullptr;
    if (kDst.x > 0 || kDst.y > 0) {
      Fill(renderer.bmp, 0);
    }

    uint32_t* const kDest = renderer.bmp.pixels +
                            static_cast<std::size_t>(kDst.y) * renderer.bmp.pitch +
                            static_cast<std::size_t>(kDst.x);
    if (kWp.w == kDst.w && kWp.h == kDst.h) {
      // BlitBitmap reads from src at position (x,y), not (0,0) — wrong for a
      // scratch bitmap whose content always starts at (0,0). Copy row-by-row.
      uint32_t const* src_row = scratch_bmp.pixels;
      uint32_t* dst_row = kDest;
      for (int row = 0; row < kWp.h; ++row) {
        std::memcpy(dst_row, src_row, sizeof(uint32_t) * static_cast<std::size_t>(kWp.w));
        dst_row += renderer.bmp.pitch;
        src_row += scratch_bmp.pitch;
      }
    } else {
      ScaleDrawArea(scratch_bmp.pixels, kWp.w, kWp.h, scratch_bmp.pitch, kDest, kDst.w, kDst.h,
                    renderer.bmp.pitch);
    }
  }  // end Composite zone

  // ── HUD overlay (native resolution, drawn on top) ─────────────────────────
  {
    ZoneScopedN("Spectator::HUD");
    for (std::size_t i = 0; i < game.worms.size(); ++i) {
      Worm const& worm = *game.worms[i];
      // Left worm (stats_x==0): left-anchored at the left edge.
      // Right worm (stats_x>0): right-anchored so the HUD sits the same
      // distance from the right edge as it does in a 320px split-screen
      // viewport (320 - stats_x - 100 = 2px gap at full health).
      int const kHudX = (worm.stats_x == 0) ? 0 : (render_w - (320 - worm.stats_x));
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
      common.font.DrawString(renderer.bmp, worm.settings->name, kHudX, renderer.render_res_y - 15,
                             7);
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
  }  // end HUD zone

  // Remember this frame's banner position and overlay size so the next frame
  // can union the banner band and detect a resolution change.
  prev_banner_y = banner_y;
  hud_overlay_w = render_w;
  hud_overlay_h = render_h;
}
