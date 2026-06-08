#include "worm.hpp"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include "console.hpp"
#include "constants.hpp"
#include "filesystem.hpp"  // For joinPath
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "io/stream.hpp"
#include "mixer/player.hpp"

#include <serialization/cereal_types.hpp>
#include <serialization/toml_archive.hpp>
#include "replay.hpp"

#include <xxhash.h>

#include <SDL3/SDL.h>

void WormSettingsExtensions::InitDefaultGamepadControls() {
  // DPad for movement
  gamepad_controls[kUp] = SDL_GAMEPAD_BUTTON_DPAD_UP;
  gamepad_controls[kDown] = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
  gamepad_controls[kLeft] = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
  gamepad_controls[kRight] = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
  // A = Jump, Right trigger = Fire, Right shoulder = Change, Left shoulder = Dig
  gamepad_controls[kJump] = SDL_GAMEPAD_BUTTON_SOUTH;
  gamepad_controls[kFire] = GamepadAxisPositive(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
  gamepad_controls[kChange] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
  gamepad_controls[kDig] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
}

struct Point {
  int x, y;
};

uint64_t& WormSettings::UpdateHash() {
  std::string toml_data = ToToml();

  hash = XXH3_64bits(toml_data.data(), toml_data.size());
  return hash;
}

std::string WormSettings::ToToml() const {
  std::ostringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);
    SerializeWormSettingsToml(ar, const_cast<WormSettings&>(*this));
  }
  return ss.str();
}

void WormSettings::FromToml(std::string const& data) {
  std::istringstream ss(data);
  cereal::TomlInputArchive ar(ss);
  SerializeWormSettingsToml(ar, *this);
}

void WormSettings::SaveProfile(const FsNode& node) {
  try {
    auto writer_ptr = node.ToWriter();
    io::Writer& writer = *writer_ptr;
    profile_node = node;

    std::string toml = ToToml();
    writer.Put(reinterpret_cast<uint8_t const*>(toml.data()), toml.size());
  } catch (std::runtime_error& e) {
    console::WriteWarning(std::string("Error saving profile: ") + e.what());
  }
}

void WormSettings::LoadProfile(const FsNode& node) {
  int const kOldColor = color;
  try {
    auto reader_ptr = node.ToReader();
    io::Reader& reader = *reader_ptr;
    profile_node = node;

    std::string content;
    uint8_t buf[4096];
    for (;;) {
      std::size_t const kGot = reader.TryGet(buf, sizeof(buf));
      if (kGot == 0) break;
      content.append(reinterpret_cast<char*>(buf), kGot);
    }
    FromToml(content);
  } catch (std::runtime_error& e) {
    console::WriteWarning(std::string("Error loading profile: ") + e.what());
  }

  color = kOldColor;  // We preserve the color
}

void Worm::CalculateReactionForce(Game& game, int new_x, int new_y, int dir) {
  static Point const kColPoints[4][7] = {{// DOWN reaction points
                                          {.x = -1, .y = -4},
                                          {.x = 0, .y = -4},
                                          {.x = 1, .y = -4},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0}},
                                         {// LEFT reaction points
                                          {.x = 1, .y = -3},
                                          {.x = 1, .y = -2},
                                          {.x = 1, .y = -1},
                                          {.x = 1, .y = 0},
                                          {.x = 1, .y = 1},
                                          {.x = 1, .y = 2},
                                          {.x = 1, .y = 3}},
                                         {// UP reaction points
                                          {.x = -1, .y = 4},
                                          {.x = 0, .y = 4},
                                          {.x = 1, .y = 4},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0},
                                          {.x = 0, .y = 0}},
                                         {// RIGHT reaction points
                                          {.x = -1, .y = -3},
                                          {.x = -1, .y = -2},
                                          {.x = -1, .y = -1},
                                          {.x = -1, .y = 0},
                                          {.x = -1, .y = 1},
                                          {.x = -1, .y = 2},
                                          {.x = -1, .y = 3}}

  };

  static int const kColPointCount[4] = {3, 7, 3, 7};

  reacts[dir] = 0;

  // newX should be x + velX at the first call

  for (int i = 0; i < kColPointCount[dir]; ++i) {
    int const kColX = new_x + kColPoints[dir][i].x;
    int const kColY = new_y + kColPoints[dir][i].y;

    if (!game.level.CheckedMatWrap(kColX, kColY).Background()) {
      ++reacts[dir];
    }
  }
}

void Worm::ProcessPhysics(Game& game) {
  Common const& common = *game.common;

  if (reacts[kRfUp] > 0) vel.x = (vel.x * LC(WormFricMult)) / LC(WormFricDiv);

  fixedvec const kAbsvel(std::abs(vel.x), std::abs(vel.y));

  int32_t rh = 0;
  int32_t rv = 0;
  int32_t mbh = 0;
  int32_t mbv = 0;

  rh = reacts[vel.x >= 0 ? kRfLeft : kRfRight];
  rv = reacts[vel.y >= 0 ? kRfUp : kRfDown];
  mbh = vel.x > 0 ? LC(MinBounceRight) : -LC(MinBounceLeft);
  mbv = vel.y > 0 ? LC(MinBounceDown) : -LC(MinBounceUp);

  if (vel.x &&
      rh)  // TODO: We wouldn't need the vel.x check if we knew that mbh/mbv were always non-zero
  {
    if (kAbsvel.x > mbh) {
      if (common.h[HFallDamage])
        health -= LC(FallDamageRight);
      else
        game.sound_player->Play(common.sound_hook[SoundBump]);
      vel.x = -vel.x / 3;
    } else {
      vel.x = 0;
    }
  }

  if (vel.y && rv) {
    if (kAbsvel.y > mbv) {
      if (common.h[HFallDamage])
        health -= LC(FallDamageDown);
      else
        game.sound_player->Play(common.sound_hook[SoundBump]);
      vel.y = -vel.y / 3;
    } else {
      vel.y = 0;
    }
  }

  if (reacts[kRfUp] == 0) {
    vel.y += LC(WormGravity);
  }

  // No, we can't use rh/rv here, they are out of date
  if (reacts[vel.x >= 0 ? kRfLeft : kRfRight] < 2) pos.x += vel.x;

  if (reacts[vel.y >= 0 ? kRfUp : kRfDown] < 2) pos.y += vel.y;
}

void Worm::Process(Game& game) {
  Common& common = *game.common;

  health = std::min(health, settings->health);

  if ((game.settings->game_mode != Settings::kGmKillEmAll &&
       game.settings->game_mode != Settings::kGmScalesOfJustice) ||
      lives > 0) {
    if (visible) {
      // Liero.exe: 291C

      auto next = pos + vel;
      auto i_next = Ftoi(next);

      {  // Calculate reaction forces

        for (int i = 0; i < 4; i++) {
          CalculateReactionForce(game, i_next.x, i_next.y, i);

          // Yes, Liero does this in every iteration. Keep it this way.

          if (i_next.x < 4) {
            reacts[kRfRight] += 5;
          } else if (i_next.x > game.level.width - 5) {
            reacts[kRfLeft] += 5;
          }

          if (i_next.y < 5) {
            reacts[kRfDown] += 5;
          } else {
            if (common.h[HWormFloat]) {
              if (i_next.y > LC(WormFloatLevel)) vel.y -= LC(WormFloatPower);
            } else if (i_next.y > game.level.height - 6) {
              reacts[kRfUp] += 5;
            }
          }
        }

        if (reacts[kRfDown] < 2) {
          if (reacts[kRfUp] > 0) {
            if (reacts[kRfLeft] > 0 || reacts[kRfRight] > 0) {
              // Low or none push down,
              // Push up and
              // Push left or right

              pos.y -= Itof(1);
              next.y = pos.y + vel.y;
              i_next.y = Ftoi(next.y);

              CalculateReactionForce(game, i_next.x, i_next.y, kRfLeft);
              CalculateReactionForce(game, i_next.x, i_next.y, kRfRight);
            }
          }
        }

        if (reacts[kRfUp] < 2) {
          if (reacts[kRfDown] > 0) {
            if (reacts[kRfLeft] > 0 || reacts[kRfRight] > 0) {
              // Low or none push up,
              // Push down and
              // Push left or right

              pos.y += Itof(1);
              next.y = pos.y + vel.y;
              i_next.y = Ftoi(next.y);

              CalculateReactionForce(game, i_next.x, i_next.y, kRfLeft);
              CalculateReactionForce(game, i_next.x, i_next.y, kRfRight);
            }
          }
        }
      }

      auto ipos = Ftoi(pos);

      auto br = game.bonuses.All();
      for (Bonus const* i = nullptr; (i = br.Next());) {
        if (ipos.x + 5 > Ftoi(i->x) && ipos.x - 5 < Ftoi(i->x) && ipos.y + 5 > Ftoi(i->y) &&
            ipos.y - 5 < Ftoi(i->y)) {
          if (i->frame == 1) {
            if (health < settings->health) {
              game.bonuses.Free(br);

              game.DoHealing(*this, (game.rand(LC(BonusHealthVar)) + LC(BonusMinHealth)) *
                                        settings->health / 100);
            }
          } else if (i->frame == 0) {
            if (game.rand(LC(BonusExplodeRisk)) > 1) {
              WormWeapon& ww = weapons[current_weapon];

              if (!common.h[HBonusReloadOnly]) {
                fire_cone = 0;

                ww.type = &common.weapons[i->weapon];
                ww.ammo = ww.type->ammo;
              }

              game.sound_player->Play(common.sound_hook[SoundReloaded]);

              game.bonuses.Free(br);

              ww.loading_left = 0;
            } else {
              int const kBix = Ftoi(i->x);
              int const kBiy = Ftoi(i->y);
              game.bonuses.Free(br);
              common.sobject_types[0].Create(game, kBix, kBiy, index, nullptr);
            }
          }
        }
      }

      ProcessSteerables(game);

      if (!movable && !Pressed(kLeft) &&
          !Pressed(kRight))  // processSteerables sets movable to false, does this interfer?
      {
        movable = true;
      }  // 2FB1

      ProcessAiming(game);
      ProcessTasks(game);
      ProcessWeapons(game);

      if (Pressed(kFire) && !Pressed(kChange) && weapons[current_weapon].Available() &&
          weapons[current_weapon].delay_left <= 0) {
        Fire(game);
      } else if (!Pressed(kFire) || Pressed(kChange) || !weapons[current_weapon].Available()) {
        if (weapons[current_weapon].type->loop_sound)
          game.sound_player->Stop(&weapons[current_weapon]);
      }

      ProcessPhysics(game);
      ProcessSight(game);

      if (Pressed(kChange)) {
        ProcessWeaponChange(game);
      } else {
        key_change_pressed = false;
        ProcessMovement(game);
      }

      if (health < settings->health / 4) {
        if (game.rand(health + 6) == 0) {
          if (game.rand(3) == 0) {
            int const kSnd =
                18 + game.rand(3);  // NOTE: MUST be outside the unpredictable branch below
            if (!game.sound_player->IsPlaying(this)) {
              game.sound_player->Play(kSnd, this);
            }
          }

          common.nobject_types[6].Create1(game, vel, pos, 0, index, nullptr);
        }
      }

      if (health <= 0) {
        leave_shell_timer = 0;
        make_sight_green = false;

        Weapon const& w = *weapons[current_weapon].type;
        if (w.loop_sound) {
          game.sound_player->Stop(&weapons[current_weapon]);
        }

        int const kDeathSnd = 15 + game.rand(3);
        game.sound_player->Play(kDeathSnd, this);

        fire_cone = 0;
        ninjarope.out = false;

        if (game.settings->game_mode == Settings::kGmScalesOfJustice) {
          while (health <= 0) {
            health += settings->health;
            --lives;
          }
        } else {
          --lives;
        }

        int const kOldLastKilled = game.last_killed_idx;
        // For GameOfTag, 'it' doesn't change if the killer
        // was not 'it', itself, unknown or there were no 'it'.
        if (game.settings->game_mode != Settings::kGmGameOfTag || game.last_killed_idx < 0 ||
            last_killed_by_idx < 0 || last_killed_by_idx == index ||
            last_killed_by_idx == game.last_killed_idx) {
          game.last_killed_idx = index;
        }
        game.got_changed = (kOldLastKilled != game.last_killed_idx);

        if (last_killed_by_idx >= 0 && last_killed_by_idx != index) {
          ++game.WormByIdx(last_killed_by_idx)->kills;
        }

        visible = false;
        killed_timer = kKilledTimerInitial;

        int const kMax = 120 * game.settings->blood / 100;

        if (kMax > 1) {
          for (int i = 1; i <= kMax; ++i) {
            common.nobject_types[6].Create2(game, game.rand(128), vel / 3, pos, 0, index, nullptr);
          }
        }

        for (int i = 7; i <= 105; i += 14) {
          common.nobject_types[index].Create2(game, i + game.rand(14), vel / 3, pos, 0, index,
                                              nullptr);
        }

        game.stats_recorder->AfterDeath(this);

        Release(kFire);
      }

      // Update frame
      int const kAnimFrame = animate ? ((game.cycles & 31) >> 3) : 0;
      current_frame = AngleFrame() + Settings::kWormAnimTab[kAnimFrame];
    } else {
      // Worm is dead
      steerable_count = 0;

      if (PressedOnce(kFire)) ready = true;

      if (killed_timer > 0) --killed_timer;

      if (killed_timer == 0 && !game.quick_sim)  // Don't respawn in quicksim
        BeginRespawn(game);

      if (killed_timer < 0) DoRespawning(game);
    }
  }
}

int Worm::AngleFrame() const {
  int x = Ftoi(aiming_angle) - 12;

  if (direction != 0) x -= 49;

  x >>= 3;
  if (x < 0)
    x = 0;
  else if (x > 6)
    x = 6;

  if (direction != 0) {
    x = 6 - x;
  }  // 9581

  return x;
}

int SqrVectorLength(int x, int y) { return x * x + y * y; }

void DumbLieroAI::Process(Game& game, Worm& worm) {
  Common const& common = *game.common;

  Worm const* target = nullptr;
  int min_len = 0;
  for (auto& i : game.worms) {
    Worm const* w = i.get();
    if (w != &worm) {
      int const kLen =
          SqrVectorLength(Ftoi(worm.pos.x) - Ftoi(w->pos.x), Ftoi(worm.pos.y) - Ftoi(w->pos.y));
      if (!target || kLen < min_len)  // First or closer worm
      {
        target = w;
        min_len = kLen;
      }
    }
  }

  int max_dist = 0;

  WormWeapon const& ww = worm.weapons[worm.current_weapon];
  Weapon const& w = *ww.type;

  if (w.time_to_explo > 0 && w.time_to_explo < 500) {
    max_dist = (w.time_to_explo - w.time_to_explo_v / 2) * w.speed / 130;
  } else {
    max_dist = w.speed - w.gravity / 10;
  }  // 4D43

  max_dist = std::max(max_dist, 90);

  fixedvec delta = target->pos - worm.pos;
  auto idelta = Ftoi(delta);

  int const kRealDist = VectorLength(idelta.x, idelta.y);

  if (kRealDist < max_dist || !worm.visible) {
    // The other worm is close enough
    bool const kFire = worm.Pressed(Worm::kFire);
    if (rand(common.ai_params.k[kFire][WormSettings::kFire]) == 0) {
      worm.SetControlState(Worm::kFire, !kFire);
    }  // 4DE7
  } else if (worm.visible) {
    worm.Release(Worm::kFire);
  }  // 4DFA

  // In Liero this is a loop with two iterations, that's better maybe
  bool const kJump = worm.Pressed(Worm::kJump);
  if (rand(common.ai_params.k[kJump][WormSettings::kJump]) == 0) {
    worm.ToggleControlState(Worm::kJump);
  }

  bool change = worm.Pressed(Worm::kChange);
  if (rand(common.ai_params.k[change][WormSettings::kChange]) == 0) {
    worm.ToggleControlState(Worm::kChange);
  }

  // l_4E6B:
  //  Moves up

  // l_4EE5:
  if (kRealDist > 0) {
    delta /= kRealDist;
  } else {
    delta.Zero();
  }  // 4F2F

  int dir = 1;

  for (; dir < 128; ++dir) {
    if (std::abs(cossin_table[dir].x - delta.x) < 0xC00 &&
        std::abs(cossin_table[dir].y - delta.y) < 0xC00)  // The original had 0xC000, which is wrong
      break;
  }  // 4F93

  fixed const kAdeltaX = std::abs(delta.x);
  fixed const kAdeltaY = std::abs(delta.y);

  if (dir >= 128) {
    if (delta.x > 0) {
      if (delta.y < 0) {
        if (kAdeltaY > kAdeltaX)
          dir = 64 + rand(16);
        else if (kAdeltaX > kAdeltaY)
          dir = 80 + rand(16);
        else
          dir = 80;
      } else  // deltaY >= 0
      {
        if (kAdeltaX > kAdeltaY)
          dir = 96 + rand(16);
        else
          dir = 116;
      }
    } else {
      if (delta.y < 0) {
        if (kAdeltaY > kAdeltaX)
          dir = 48 + rand(16);
        else if (kAdeltaX > kAdeltaY)
          dir = 32 + rand(16);
        else
          dir = 48;  // This was 56, but that seems wrong
      } else         // deltaX <= 0 && deltaY >= 0
      {
        if (kAdeltaX > kAdeltaY)
          dir = 12 + rand(16);
        else
          dir = 12;
      }
    }
  }  // 50FD

  /* TODO (maybe)
     if(realdist < maxdist)
     {
          if(dir < 64)
          {
   l_510E:
           //What the hell is wrong with this code?
           //It is messed up totaly! Translating the correct code
           //NOTE! Something has to be done here!
           dir += ax; //What the hell is AX?
           if(dir > 64)
           {
            dir = 64;
           }
          } // 5167
          if(dir > 64)
          {
           //The same thing with this code! Is it encrypted or what?
           dir -= ax; //Again
           if(dir < 64)
           {
            dir = 64;
           }
          }
     } // 51C6
  */

  change = worm.Pressed(Worm::kChange);

  if (change) {
    if (rand(common.ai_params.k[worm.Pressed(Worm::kLeft)][WormSettings::kLeft]) == 0) {
      worm.ToggleControlState(Worm::kLeft);
    }

    if (rand(common.ai_params.k[worm.Pressed(Worm::kRight)][WormSettings::kRight]) == 0) {
      worm.ToggleControlState(Worm::kRight);
    }

    if (worm.ninjarope.out && worm.ninjarope.attached) {
      // l_525F:
      bool const kUp = worm.Pressed(Worm::kUp);

      if (rand(common.ai_params.k[kUp][WormSettings::kUp]) == 0) {
        worm.ToggleControlState(Worm::kUp);
      }

      bool const kDown = worm.Pressed(Worm::kDown);
      if (rand(common.ai_params.k[kDown][WormSettings::kDown]) == 0) {
        worm.ToggleControlState(Worm::kDown);
      }
    } else {
      // l_52D2:
      worm.Release(Worm::kUp);
      worm.Release(Worm::kDown);
    }  // 52F8
  }  // if(change)
  else {
    if (kRealDist > max_dist) {
      worm.SetControlState(Worm::kRight, (delta.x > 0));
      worm.SetControlState(Worm::kLeft, (delta.x <= 0));
    }  // 5347
    else {
      worm.Release(Worm::kRight);
      worm.Release(Worm::kLeft);
    }

    if (worm.direction != 0) {
      if (dir < 64) worm.Press(Worm::kLeft);
      // 5369
      worm.SetControlState(Worm::kUp, (dir + 1 < Ftoi(worm.aiming_angle)));
      // 5379
      worm.SetControlState(Worm::kDown, (dir - 1 > Ftoi(worm.aiming_angle)));
    } else {
      if (dir > 64) worm.Press(Worm::kRight);
      // 53C6
      worm.SetControlState(Worm::kUp, (dir - 1 > Ftoi(worm.aiming_angle)));
      // 53E8
      worm.SetControlState(Worm::kDown, (dir + 1 < Ftoi(worm.aiming_angle)));
      // 540A
    }

    if (worm.Pressed(Worm::kLeft) && worm.reacts[Worm::kRfRight]) {
      if (worm.reacts[Worm::kRfDown] > 0)
        worm.Press(Worm::kRight);
      else
        worm.Press(Worm::kJump);
    }  // 5454

    if (worm.Pressed(Worm::kRight) && worm.reacts[Worm::kRfLeft]) {
      if (worm.reacts[Worm::kRfDown] > 0)
        worm.Press(Worm::kLeft);
      else
        worm.Press(Worm::kJump);
    }  // 549E
  }
}

void Worm::InitWeapons(Game& game) {
  Common& common = *game.common;
  current_weapon = 0;  // It was 1 in OpenLiero A1

  for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
    WormWeapon& ww = weapons[j];
    ww.type = &common.weapons[common.weap_order[settings->weapons[j] - 1]];
    ww.ammo = ww.type->ammo;
    ww.delay_left = 0;
    ww.loading_left = 0;
  }
}

void Worm::BeginRespawn(Game& game) {
  Common const& common = *game.common;

  auto temp = Ftoi(pos);

  logic_respawn = temp - IVec2(80, 80);

  auto enemy = temp;

  if (game.worms.size() == 2) {
    enemy = Ftoi(game.worms[index ^ 1]->pos);
  }

  int trials = 0;
  do {
    pos.x = Itof(LC(WormSpawnRectX) + game.rand(LC(WormSpawnRectW)));
    pos.y = Itof(LC(WormSpawnRectY) + game.rand(LC(WormSpawnRectH)));

    // The original didn't have + 4 in both, which seems
    // to be done in the exe and makes sense.
    while (Ftoi(pos.y) + 4 < game.level.height &&
           game.level.Mat(Ftoi(pos.x), Ftoi(pos.y) + 4).Background()) {
      pos.y += Itof(1);
    }

    if (++trials >= 50000) break;
  } while (!CheckRespawnPosition(game, enemy.x, enemy.y, temp.x, temp.y, Ftoi(pos.x), Ftoi(pos.y)));

  killed_timer = -1;
}

static void LimitXy(int& x, int& y, int max_x, int max_y) {
  if (x < 0)
    x = 0;
  else if (x > max_x)
    x = max_x;

  y = std::max(y, 0);
  y = std::min(y, max_y);
}

void Worm::DoRespawning(Game& game) {
  Common& common = *game.common;

  for (int c = 0; c < 4; c++) {
    if (logic_respawn.x < Ftoi(pos.x) - 80)
      ++logic_respawn.x;
    else if (logic_respawn.x > Ftoi(pos.x) - 80)
      --logic_respawn.x;

    if (logic_respawn.y < Ftoi(pos.y) - 80)
      ++logic_respawn.y;
    else if (logic_respawn.y > Ftoi(pos.y) - 80)
      --logic_respawn.y;
  }

  LimitXy(logic_respawn.x, logic_respawn.y, game.level.width - 158, game.level.height - 158);

  int dest_x = Ftoi(pos.x) - 80;
  int dest_y = Ftoi(pos.y) - 80;
  LimitXy(dest_x, dest_y, game.level.width - 158, game.level.height - 158);

  if (logic_respawn.x < dest_x + 5 && logic_respawn.x > dest_x - 5 &&
      logic_respawn.y < dest_y + 5 && logic_respawn.y > dest_y - 5 &&
      ready)  // Don't spawn in quicksim
  {
    auto ipos = Ftoi(pos);
    DrawDirtEffect(common, game.rand, game.level, 0, ipos.x - 7, ipos.y - 7);
    if (game.settings->shadow)
      CorrectShadow(common, game.level, Rect(ipos.x - 10, ipos.y - 10, ipos.x + 11, ipos.y + 11));

    ready = false;
    game.sound_player->Play(common.sound_hook[SoundAlive]);

    visible = true;
    fire_cone = 0;
    vel.Zero();
    if (game.settings->game_mode != Settings::kGmScalesOfJustice) health = settings->health;

    // NOTE: This was done at death before, but doing it here seems to make more sense
    if (game.rand() & 1) {
      aiming_angle = Itof(32);
      direction = 0;
    } else {
      aiming_angle = Itof(96);
      direction = 1;
    }

    game.stats_recorder->AfterSpawn(this);
  }
}

void Worm::ProcessWeapons(Game& game) {
  Common& common = *game.common;

  for (auto& weapon : weapons) {
    if (weapon.delay_left >= 0) --weapon.delay_left;
  }

  WormWeapon& ww = weapons[current_weapon];
  Weapon const& w = *ww.type;

  if (ww.ammo <= 0) {
    int const kComputedLoadingTime = w.ComputedLoadingTime(*game.settings);
    ww.loading_left = kComputedLoadingTime;
    ww.ammo = w.ammo;
  }

  if (ww.loading_left > 0)  // NOTE: computedLoadingTime is never 0, so this works
  {
    --ww.loading_left;
    if (ww.loading_left <= 0 && w.play_reload_sound) {
      game.sound_player->Play(common.sound_hook[SoundReloaded]);
    }
  }

  if (fire_cone > 0) {
    --fire_cone;
  }

  if (leave_shell_timer > 0) {
    if (--leave_shell_timer <= 0) {
      auto vel_y = -static_cast<int>(game.rand(20000));
      auto vel_x = game.rand(16000) - 8000;
      common.nobject_types[7].Create1(game, fixedvec(vel_x, vel_y), pos, 0, index, nullptr);
    }
  }
}

void Worm::ProcessMovement(Game& game) {
  Common& common = *game.common;

  if (movable) {
    bool const kLeft = Pressed(Worm::kLeft);
    bool const kRight = Pressed(Worm::kRight);

    if (kLeft && !kRight) {
      if (vel.x > LC(MaxVelLeft)) vel.x -= LC(WalkVelLeft);

      if (direction != 0) {
        aiming_speed = 0;
        if (aiming_angle >= Itof(64)) aiming_angle = Itof(128) - aiming_angle;
        direction = 0;
      }

      animate = true;
    }

    if (!kLeft && kRight) {
      if (vel.x < LC(MaxVelRight)) vel.x += LC(WalkVelRight);

      if (direction != 1) {
        aiming_speed = 0;
        if (aiming_angle <= Itof(64)) aiming_angle = Itof(128) - aiming_angle;
        direction = 1;
      }

      animate = true;
    }

    if (kLeft && kRight) {
      if (able_to_dig) {
        able_to_dig = false;

        fixedvec const kDir(cossin_table[Ftoi(aiming_angle)]);

        auto dig_pos = kDir * 2 + pos;

        /* TODO
        long iDigx = ftoi(fTempx) - 4;
        if(iDigx < 0)    iDigx = 0;
        if(iDigx >= levwidth) iDigx = levwidth-1;

        long iDigenx = ftoi(fTempx) + 4;
        if(iDigenx < 0)    iDigenx = 0;
        if(iDigenx >= levwidth) iDigenx = levwidth-1;

        long iDigy;

        long iDigsty = ftoi(fTempy) - 4;
        if(iDigsty < 0)    iDigsty = 0;
        if(iDigsty >= levheight) iDigsty = levheight-1;

        long iDigeny = ftoi(fTempy) + 4;
        if(iDigeny < 0)    iDigeny = 0;
        if(iDigeny >= levheight) iDigeny = levheight-1;

        for(; iDigx <= iDigenx; iDigx++)
        {
                for(iDigy = iDigsty; iDigy <= iDigeny; iDigy++)
                {
                        //Throw away every third pixel
                        if(materials.Dirt[lev(iDigx, iDigy)] && random(3) == 0)
                        {
                                CreateObject2(random(128), 0, 0, itof(iDigx), itof(iDigy),
        lev(iDigx, iDigy), 2, BYTE(w)); } // 419A } // 41A9 } // 41BB
*/

        dig_pos.x -= Itof(7);
        dig_pos.y -= Itof(7);

        auto idig_pos = Ftoi(dig_pos);
        DrawDirtEffect(common, game.rand, game.level, 7, idig_pos.x, idig_pos.y);
        if (game.settings->shadow)
          CorrectShadow(common, game.level,
                        Rect(idig_pos.x - 3, idig_pos.y - 3, idig_pos.x + 18, idig_pos.y + 18));

        dig_pos += kDir * 2;

        // l_43EB:
        idig_pos = Ftoi(dig_pos);
        DrawDirtEffect(common, game.rand, game.level, 7, idig_pos.x, idig_pos.y);
        if (game.settings->shadow)
          CorrectShadow(common, game.level,
                        Rect(idig_pos.x - 3, idig_pos.y - 3, idig_pos.x + 18, idig_pos.y + 18));

        // NOTE! Maybe the shadow corrections can be joined into one? Mmm?
      }  // 4552
    } else {
      able_to_dig = true;
    }

    if (!kLeft && !kRight) {
      animate = false;  // Don't animate the this unless he is moving
    }  // 458C
  }
}

void Worm::ProcessTasks(Game& game) {
  Common const& common = *game.common;

  if (Pressed(kChange)) {
    if (ninjarope.out) {
      if (Pressed(kUp)) ninjarope.length -= LC(NRPullVel);
      if (Pressed(kDown)) ninjarope.length += LC(NRReleaseVel);

      if (ninjarope.length < LC(NRMinLength)) ninjarope.length = LC(NRMinLength);
      if (ninjarope.length > LC(NRMaxLength)) ninjarope.length = LC(NRMaxLength);
    }

    if (PressedOnce(kJump)) {
      ninjarope.out = true;
      ninjarope.attached = false;

      game.sound_player->Play(common.sound_hook[SoundNinjaropeThrow]);

      ninjarope.pos = pos;
      ninjarope.vel = fixedvec(cossin_table[Ftoi(aiming_angle)].x << LC(NRThrowVelX),
                               cossin_table[Ftoi(aiming_angle)].y << LC(NRThrowVelY));

      ninjarope.length = LC(NRInitialLength);
    }
  } else {
    // Jump = remove ninjarope, jump
    if (Pressed(kJump)) {
      ninjarope.out = false;
      ninjarope.attached = false;

      if ((reacts[kRfUp] > 0 || common.h[HAirJump]) && (able_to_jump || common.h[HMultiJump])) {
        vel.y -= LC(JumpForce);
        able_to_jump = false;
      }
    } else {
      able_to_jump = true;
    }
  }
}

void Worm::ProcessAiming(Game& game) {
  Common const& common = *game.common;

  bool const kUp = Pressed(Worm::kUp);
  bool const kDown = Pressed(Worm::kDown);

  if (aiming_speed != 0) {
    aiming_angle += aiming_speed;

    if (!kUp && !kDown) {
      aiming_speed = (aiming_speed * LC(AimFricMult)) / LC(AimFricDiv);
    }

    if (direction == 1) {
      if (Ftoi(aiming_angle) > LC(AimMaxRight)) {
        aiming_speed = 0;
        aiming_angle = Itof(LC(AimMaxRight));
      }
      if (Ftoi(aiming_angle) < LC(AimMinRight)) {
        aiming_speed = 0;
        aiming_angle = Itof(LC(AimMinRight));
      }
    } else {
      if (Ftoi(aiming_angle) < LC(AimMaxLeft)) {
        aiming_speed = 0;
        aiming_angle = Itof(LC(AimMaxLeft));
      }
      if (Ftoi(aiming_angle) > LC(AimMinLeft)) {
        aiming_speed = 0;
        aiming_angle = Itof(LC(AimMinLeft));
      }
    }
  }

  if (movable && (!ninjarope.out || !Pressed(kChange))) {
    if (kUp) {
      if (direction == 0) {
        if (aiming_speed < LC(MaxAimVelLeft)) aiming_speed += LC(AimAccLeft);
      } else {
        if (aiming_speed > LC(MaxAimVelRight)) aiming_speed -= LC(AimAccRight);
      }
    }

    if (kDown) {
      if (direction == 1) {
        if (aiming_speed < LC(MaxAimVelLeft)) aiming_speed += LC(AimAccLeft);
      } else {
        if (aiming_speed > LC(MaxAimVelRight)) aiming_speed -= LC(AimAccRight);
      }
    }
  }
}

void Worm::ProcessWeaponChange(Game& game) {
  if (!key_change_pressed) {
    Release(kLeft);
    Release(kRight);

    key_change_pressed = true;
  }

  fire_cone = 0;
  animate = false;

  if (weapons[current_weapon].type->loop_sound) {
    game.sound_player->Stop(&weapons[current_weapon]);
  }

  if (weapons[current_weapon].Available() || game.settings->load_change) {
    if (PressedOnce(kLeft)) {
      if (--current_weapon < 0) current_weapon = Settings::kSelectableWeapons - 1;

      hotspot_x = Ftoi(pos.x);
      hotspot_y = Ftoi(pos.y);
    }

    if (PressedOnce(kRight)) {
      if (++current_weapon >= Settings::kSelectableWeapons) current_weapon = 0;

      hotspot_x = Ftoi(pos.x);
      hotspot_y = Ftoi(pos.y);
    }
  }
}

void Worm::Fire(Game& game) {
  Common const& common = *game.common;
  WormWeapon& ww = weapons[current_weapon];
  Weapon const& w = *ww.type;

  --ww.ammo;
  ww.delay_left = w.delay;

  fire_cone = w.fire_cone;

  fixedvec const kFiring(cossin_table[Ftoi(aiming_angle)] * (w.detect_distance + 5) + pos -
                         fixedvec(0, Itof(1)));

  if (w.leave_shells > 0) {
    if (game.rand(w.leave_shells) == 0) {
      leave_shell_timer = w.leave_shell_delay;
    }
  }

  if (w.loop_sound) {
    if (!game.sound_player->IsPlaying(&weapons[current_weapon])) {
      game.sound_player->Play(w.launch_sound, &weapons[current_weapon], -1);
    }
  } else {
    game.sound_player->Play(w.launch_sound);
  }

  int speed = w.speed;
  fixedvec firing_vel;
  int const kParts = w.parts;

  if (w.affect_by_worm) {
    speed = std::max(speed, 100);

    firing_vel = vel * 100 / speed;
  }

  for (int i = 0; i < kParts; ++i) {
    w.Fire(game, Ftoi(aiming_angle), firing_vel, speed, kFiring, index, &ww);
  }

  int recoil = w.recoil;

  if (common.h[HSignedRecoil] && recoil >= 128) recoil -= 256;

  vel -= cossin_table[Ftoi(aiming_angle)] * recoil / 100;
}

bool CheckForWormHit(Game& game, int x, int y, int dist, Worm* own_worm) {
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm& w = *game.worms[i];

    if (&w != own_worm) {
      return CheckForSpecWormHit(game, x, y, dist, w);
    }
  }

  return false;
}

bool CheckForSpecWormHit(Game& game, int x, int y, int dist, Worm& w) {
  Common& common = *game.common;

  if (!w.visible) return false;

  PalIdx const* worm_sprite = common.WormSprite(w.current_frame, w.direction, 0);

  int const kDeltaX = x - Ftoi(w.pos.x) + 7;
  int const kDeltaY = y - Ftoi(w.pos.y) + 5;

  Rect r(kDeltaX - dist, kDeltaY - dist, kDeltaX + dist + 1, kDeltaY + dist + 1);

  r.Intersect(Rect(0, 0, 16, 16));

  for (int cy = r.y1; cy < r.y2; ++cy)
    for (int cx = r.x1; cx < r.x2; ++cx) {
      assert(cy * 16 + cx < 16 * 16);
      if (common.materials[worm_sprite[cy * 16 + cx]].Worm()) return true;
    }

  return false;
}

void Worm::ProcessSight(Game& game) {
  Common& common = *game.common;

  WormWeapon const& ww = weapons[current_weapon];
  Weapon const& w = *ww.type;

  if (ww.Available() && (w.laser_sight || ww.type - common.weapons.data() == LC(LaserWeapon) - 1)) {
    fixedvec const kDir = cossin_table[Ftoi(aiming_angle)];
    fixedvec temp = fixedvec(pos.x + kDir.x * 6, pos.y + kDir.y * 6 - Itof(1));

    do {
      temp += kDir;
      make_sight_green = CheckForWormHit(game, Ftoi(temp.x), Ftoi(temp.y), 0, this);
    } while (temp.x >= 0 && temp.y >= 0 && temp.x < Itof(game.level.width) &&
             temp.y < Itof(game.level.height) && game.level.Mat(Ftoi(temp)).Background() &&
             !make_sight_green);

    hotspot_x = Ftoi(temp.x);
    hotspot_y = Ftoi(temp.y);
  } else {
    make_sight_green = false;
  }
}

void Worm::ProcessSteerables(Game& game) {
  steerable_count = 0;
  steerable_sum_x = 0;
  steerable_sum_y = 0;

  WormWeapon const& ww = weapons[current_weapon];
  if (ww.type->shot_type == Weapon::kStSteerable) {
    auto wr = game.wobjects.All();
    for (WObject* i = nullptr; (i = wr.Next());) {
      if (i->type == ww.type && i->owner_idx == index) {
        if (Pressed(kLeft)) i->cur_frame -= (game.cycles & 1) + 1;

        if (Pressed(kRight)) i->cur_frame += (game.cycles & 1) + 1;

        i->cur_frame &= 127;  // Wrap
        movable = false;

        steerable_sum_x += Ftoi(i->pos.x);
        steerable_sum_y += Ftoi(i->pos.y);
        ++steerable_count;
      }
    }
  }
}
