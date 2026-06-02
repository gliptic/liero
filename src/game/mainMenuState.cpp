#include "mainMenuState.hpp"

#include "controller/controller.hpp"
#include "fileSelectorState.hpp"
#include "filesystem.hpp"
#include "gfx.hpp"
#include "inputState.hpp"
#include "keys.hpp"
#include "level.hpp"
#include "menu/mainMenu.hpp"
#include "mixer/player.hpp"
#include "net/session.hpp"
#include "rand.hpp"
#include "text.hpp"
#include "weaponMenuState.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

using std::vector;

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

static int Levenshtein(char const* s1, char const* s2) {
  std::size_t x, y, s1len, s2len;
  s1len = strlen(s1);
  s2len = strlen(s2);
  std::size_t w = s1len + 1;
  std::vector<unsigned> matrix(w * (s2len + 1));
  matrix[0] = 0;
  for (x = 1; x <= s2len; x++) matrix[x * w] = matrix[(x - 1) * w] + 1;
  for (y = 1; y <= s1len; y++) matrix[y] = matrix[y - 1] + 1;
  for (x = 1; x <= s2len; x++)
    for (y = 1; y <= s1len; y++) {
      int c = std::tolower(s1[y - 1]) == std::tolower(s2[x - 1]) ? 0 : 1;
      matrix[x * w + y] = MIN3(matrix[(x - 1) * w + y] + 1, matrix[x * w + y - 1] + 1,
                               matrix[(x - 1) * w + y - 1] + c);
    }
  return (int)(matrix[s2len * w + s1len]);
}

#undef MIN3

static void ResetLeftRight() {
  gfx.ReleaseSdlKey(SDL_SCANCODE_LEFT);
  gfx.ReleaseSdlKey(SDL_SCANCODE_RIGHT);
  gfx.ReleaseControl(WormSettingsExtensions::kLeft);
  gfx.ReleaseControl(WormSettingsExtensions::kRight);
}

// Builds an InputStringState for a Save As dialog with name-collision
// handling: if the typed name would shadow a shipped file or an
// auto-managed file, an info box is shown and the input is re-opened
// preserving what the user typed. `onComplete` is called once at the
// end of the dialog flow — with the validated name on success, or an
// empty string on cancel.
static std::unique_ptr<AppState> MakeSaveAsState(
    std::string subdir, std::string ext, std::string initial, int x, int y,
    std::function<void(std::string const& result)> on_complete) {
  auto cb = [subdir, ext, x, y, on_complete](bool accepted, std::string const& result) {
    if (!accepted || result.empty()) {
      on_complete(std::string());
      return;
    }
    std::string leaf = result + ext;
    if (paths::ShadowsSystem(gfx.GetUserConfigNode(), subdir, leaf)) {
      std::string msg = "NAME '" + leaf + "' IS RESERVED";
      gfx.state_stack.ScheduleReplaceTop(std::make_unique<InfoBoxState>(
          msg, 160, 100, true, [subdir, ext, result, x, y, on_complete]() {
            gfx.state_stack.ScheduleReplaceTop(
                MakeSaveAsState(subdir, ext, result, x, y, on_complete));
          }));
      return;
    }
    on_complete(result);
  };
  return std::make_unique<InputStringState>(std::move(initial), 30, x, y, nullptr, "", false,
                                            std::move(cb));
}

MainMenuState::MainMenuState() {}

void MainMenuState::Enter() {
  Common& common = *gfx->common;
  int center_x = gfx->single_screen_renderer.render_res_x / 2;

  std::memset(gfx->play_renderer.pal.entries, 0, sizeof(gfx->play_renderer.pal.entries));
  std::memset(gfx->single_screen_renderer.pal.entries, 0,
              sizeof(gfx->single_screen_renderer.pal.entries));
  gfx->Flip();
  gfx->Process();

  FillRect(gfx->play_renderer.bmp, 0, 151, 160, 7, 0);
  common.font.DrawString(gfx->play_renderer.bmp, LS(Copyright2), 2, 152, 19);

  if (gfx->controller->Running()) {
    gfx->main_menu.SetVisibility(MainMenu::kMaResumeGame, true);
    gfx->main_menu.ItemFromId(MainMenu::kMaResumeGame)->string = "RESUME GAME (F1)";
    gfx->main_menu.ItemFromId(MainMenu::kMaNewGame)->string = "NEW GAME";
    startItemId_ = MainMenu::kMaResumeGame;
  } else {
    gfx->main_menu.SetVisibility(MainMenu::kMaResumeGame, false);
    gfx->main_menu.ItemFromId(MainMenu::kMaNewGame)->string = "NEW GAME (F1)";
    startItemId_ = MainMenu::kMaNewGame;
  }

  gfx->main_menu.ItemFromId(MainMenu::kMaTc)->string = "TC (" + gfx->settings->tc + ")";

  gfx->main_menu.MoveToFirstVisible();
  gfx->settings_menu.MoveToFirstVisible();
  gfx->settings_menu.UpdateItems(common);

  gfx->play_renderer.fade_value = 0;
  gfx->single_screen_renderer.fade_value = 0;
  gfx->cur_menu = &gfx->main_menu;

  gfx->frozen_screen.Copy(gfx->play_renderer.bmp);
  gfx->single_screen_renderer.Clear();
  if (gfx->controller->CurrentLevel()) {
    gfx->controller->CurrentLevel()->DrawMiniature(gfx->single_screen_renderer.bmp, center_x - 126,
                                                   gfx->single_screen_renderer.render_res_y - 208,
                                                   2);
  }
  gfx->frozen_spectator_screen.Copy(gfx->single_screen_renderer.bmp);

  gfx->menu_cycles = 0;
  selected_ = -1;
  phase_ = Phase::kActive;
}

void MainMenuState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool MainMenuState::Update() {
  if (phase_ == Phase::kFadingOut) {
    if (gfx->play_renderer.fade_value > 0) {
      --gfx->play_renderer.fade_value;
      --gfx->single_screen_renderer.fade_value;
      return true;
    }
    return false;
  }

  // Check if a sub-state left a result for us
  if (gfx->pending_menu_selection >= 0) {
    selected_ = gfx->pending_menu_selection;
    gfx->pending_menu_selection = -1;
  }

  // Phase::Active — process input
  Common& common = *gfx->common;

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) ||
      gfx->TestControlOnce(WormSettingsExtensions::kJump) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST)) {
    if (gfx->cur_menu == &gfx->main_menu)
      gfx->main_menu.MoveToId(MainMenu::kMaQuit);
    else
      gfx->cur_menu = &gfx->main_menu;
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_UP) || gfx->TestControlOnce(WormSettingsExtensions::kUp) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
    gfx->cur_menu->Movement(-1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_DOWN) ||
      gfx->TestControlOnce(WormSettingsExtensions::kDown) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
    gfx->cur_menu->Movement(1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RETURN) || gfx->TestSdlKeyOnce(SDL_SCANCODE_KP_ENTER) ||
      gfx->TestControlOnce(WormSettingsExtensions::kFire) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
    if (gfx->cur_menu == &gfx->main_menu) {
      g_sound_player->Play(common.sound_hook[SoundMenuSelect]);

      int s = gfx->main_menu.SelectedId();
      switch (s) {
        case MainMenu::kMaSettings: {
          gfx->cur_menu = &gfx->settings_menu;
          break;
        }

        case MainMenu::kMaPlayer1Settings:
        case MainMenu::kMaPlayer2Settings: {
          gfx->PlayerSettings(s - MainMenu::kMaPlayer1Settings);
          break;
        }

        case MainMenu::kMaNetPlayerSettings: {
          gfx->PlayerSettings(Settings::kNetworkPlayerIdx);
          break;
        }

        case MainMenu::kMaAdvanced: {
          gfx->OpenHiddenMenu();
          break;
        }

        case MainMenu::kMaReplays: {
          gfx->state_stack.Push(std::make_unique<ReplaySelectorState>(), gfx);
          break;
        }

        case MainMenu::kMaTc: {
          gfx->state_stack.Push(std::make_unique<TcSelectorState>(), gfx);
          break;
        }

        case MainMenu::kMaJoinGame: {
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->state_stack.Push(std::make_unique<InputStringState>(
                                    "", 40, 10, 80, nullptr, "ADDRESS: ", false,
                                    [this](bool accepted, std::string const& result) {
                                      if (accepted && !result.empty()) {
                                        gfx->pending_net_address = result;
                                        gfx->pending_menu_selection = MainMenu::kMaJoinGame;
                                      }
                                    }),
                                gfx);
          break;
        }

        case MainMenu::kMaHostOnline: {
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->pending_menu_selection = MainMenu::kMaHostOnline;
          break;
        }

        case MainMenu::kMaJoinOnline: {
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->state_stack.Push(std::make_unique<InputStringState>(
                                    "", 6, 10, 80, ::toupper, "ROOM CODE: ", false,
                                    [this](bool accepted, std::string const& result) {
                                      if (accepted && result.size() == 6) {
                                        gfx->pending_net_address = result;
                                        gfx->pending_menu_selection = MainMenu::kMaJoinOnline;
                                      }
                                    }),
                                gfx);
          break;
        }

        default: {
          gfx->cur_menu = &gfx->main_menu;
          selected_ = s;
        }
      }
    } else if (gfx->cur_menu == &gfx->settings_menu) {
      int item_id = gfx->settings_menu.SelectedId();
      switch (item_id) {
        case SettingsMenu::kSiLevel:
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->state_stack.Push(std::make_unique<LevelSelectorState>(), gfx);
          break;

        case SettingsMenu::kSiWeaponOptions:
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->state_stack.Push(std::make_unique<WeaponMenuState>(), gfx);
          break;

        case SettingsMenu::kLoadOptions:
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          gfx->state_stack.Push(std::make_unique<OptionsSelectorState>(), gfx);
          break;

        case SettingsMenu::kSaveOptions: {
          g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
          int x, y;
          auto* item = gfx->settings_menu.ItemFromId(SettingsMenu::kSaveOptions);
          if (item && gfx->settings_menu.ItemPosition(*item, x, y)) {
            x += gfx->settings_menu.value_offset_x + 2;
            std::string name = GetBasename(GetLeaf(gfx->settings_node.FullPath()));
            gfx->state_stack.Push(
                MakeSaveAsState("Setups", ".cfg", name, x, y,
                                [this](std::string const& result) {
                                  if (!result.empty()) {
                                    gfx->SaveSettings(gfx->GetUserConfigNode() / "Setups" /
                                                      (result + ".cfg"));
                                  }
                                  g_sound_player->Play(gfx->common->sound_hook[SoundMenuSelect]);
                                  gfx->settings_menu.UpdateItems(*gfx->common);
                                }),
                gfx);
          }
          break;
        }

        default:
          gfx->settings_menu.OnEnter(common);
          break;
      }
    } else if (gfx->cur_menu == &gfx->player_menu) {
      int item_id = gfx->player_menu.SelectedId();

      if (item_id == PlayerMenu::kPlLoadProfile) {
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        gfx->state_stack.Push(std::make_unique<ProfileSelectorState>(*gfx->player_menu.ws), gfx);
      } else if (item_id == PlayerMenu::kPlName) {
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        auto& ws = *gfx->player_menu.ws;
        int x, y;
        auto* item = gfx->player_menu.ItemFromId(item_id);
        if (item && gfx->player_menu.ItemPosition(*item, x, y)) {
          x += gfx->player_menu.value_offset_x + 2;
          gfx->state_stack.Push(
              std::make_unique<InputStringState>(ws.name, 20, x, y, nullptr, "", false,
                                                 [this](bool accepted, std::string const& result) {
                                                   auto& ws = *gfx->player_menu.ws;
                                                   if (accepted) ws.name = result;
                                                   if (ws.name.empty())
                                                     Settings::GenerateName(ws, gfx->rand);
                                                   ws.random_name = false;
                                                   g_sound_player->Play(
                                                       gfx->common->sound_hook[SoundMenuSelect]);
                                                   gfx->player_menu.UpdateItems(*gfx->common);
                                                 }),
              gfx);
        }
      } else if (item_id == PlayerMenu::kPlSaveProfileAs) {
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        int x, y;
        auto* item = gfx->player_menu.ItemFromId(item_id);
        if (item && gfx->player_menu.ItemPosition(*item, x, y)) {
          x += gfx->player_menu.value_offset_x + 2;
          gfx->state_stack.Push(
              MakeSaveAsState("Profiles", ".toml", "", x, y,
                              [this](std::string const& result) {
                                if (!result.empty()) {
                                  gfx->player_menu.ws->SaveProfile(gfx->GetUserConfigNode() /
                                                                   "Profiles" / (result + ".toml"));
                                }
                                g_sound_player->Play(gfx->common->sound_hook[SoundMenuSelect]);
                                gfx->player_menu.UpdateItems(*gfx->common);
                              }),
              gfx);
        }
      } else if ((item_id >= PlayerMenu::kPlUp && item_id <= PlayerMenu::kPlJump) ||
                 item_id == PlayerMenu::kPlDig) {
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        bool extended = gfx->settings->kExtensions;
        int key_idx = item_id - PlayerMenu::kPlUp;

        gfx->state_stack.Push(
            std::make_unique<WaitForKeyState>(extended,
                                              [this, key_idx](uint32_t k, bool is_gamepad) {
                                                auto& ws = *gfx->player_menu.ws;
                                                if (k != kDkEscape) {
                                                  if (is_gamepad) {
                                                    ws.gamepad_controls[key_idx] = k;
                                                  } else {
                                                    if (!IsExtendedKey(k)) ws.controls[key_idx] = k;
                                                    ws.controls_ex[key_idx] = k;
                                                  }
                                                  gfx->player_menu.UpdateItems(*gfx->common);
                                                }
                                              }),
            gfx);
      } else if (item_id >= PlayerMenu::kPlWeap0 && item_id < PlayerMenu::kPlWeap0 + 5) {
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        int x, y;
        auto* item = gfx->player_menu.ItemFromId(item_id);
        if (item && gfx->player_menu.ItemPosition(*item, x, y)) {
          x += gfx->player_menu.value_offset_x + 2;
          int weap_idx = item_id - PlayerMenu::kPlWeap0;
          gfx->state_stack.Push(std::make_unique<InputStringState>(
                                    "", 10, x, y, nullptr, "", false,
                                    [this, weap_idx](bool accepted, std::string const& result) {
                                      if (accepted && !result.empty()) {
                                        Common& common = *gfx->common;
                                        auto& ws = *gfx->player_menu.ws;
                                        uint32_t num_weapons = (uint32_t)common.weapons.size();

                                        uint32_t best = ws.weapons[weap_idx];
                                        double best_dist = std::numeric_limits<double>::max();
                                        for (uint32_t i = 1; i <= num_weapons; ++i) {
                                          std::string& name =
                                              common.weapons[common.weap_order[i - 1]].name;
                                          double dist = Levenshtein(name.c_str(), result.c_str()) /
                                                        (double)name.length();
                                          if (dist < best_dist) {
                                            best = i;
                                            best_dist = dist;
                                          }
                                        }
                                        ws.weapons[weap_idx] = best;
                                        gfx->player_menu.UpdateItems(common);
                                      }
                                    }),
                                gfx);
        }
      } else {
        selected_ = gfx->cur_menu->OnEnter(common);
      }
    } else {
      selected_ = gfx->cur_menu->OnEnter(common);
    }
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F1)) {
    gfx->cur_menu = &gfx->main_menu;
    gfx->main_menu.MoveToId(startItemId_);
    selected_ = startItemId_;
  }
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F2)) {
    gfx->main_menu.MoveToId(MainMenu::kMaAdvanced);
    gfx->OpenHiddenMenu();
  }
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F3)) {
    gfx->cur_menu = &gfx->main_menu;
    gfx->main_menu.MoveToId(MainMenu::kMaReplays);
    gfx->state_stack.Push(std::make_unique<ReplaySelectorState>(), gfx);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F5)) {
    gfx->main_menu.MoveToId(MainMenu::kMaPlayer1Settings);
    gfx->PlayerSettings(0);
  }
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F6)) {
    gfx->main_menu.MoveToId(MainMenu::kMaPlayer2Settings);
    gfx->PlayerSettings(1);
  }
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F7)) {
    gfx->main_menu.MoveToId(MainMenu::kMaSettings);
    gfx->cur_menu = &gfx->settings_menu;
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F9)) {
    gfx->main_menu.MoveToId(MainMenu::kMaNetPlayerSettings);
    gfx->PlayerSettings(Settings::kNetworkPlayerIdx);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_F8)) {
    uint32_t s = 14;

    Rand r;
    r.Seed(s);

    Common& common = *gfx->common;

    vector<std::size_t> nobj_map;

    for (std::size_t i = 0; i < common.nobject_types.size(); ++i) {
      nobj_map.push_back(i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(nobj_map.begin(), nobj_map.end(), g);

    for (auto& w : common.weapons) {
      w.add_speed = r(30) - 5;
      w.affect_by_explosions = r(2) == 0;
      w.affect_by_worm = r(3) == 0;
      w.ammo = r(20) + 1;
      w.blood_on_hit = r(50);
      w.blow_away = r(10);
      w.bounce = r(90);
      w.collide_with_objects = r(10) == 0;
      w.color_bullets = 3 + r(250);
      w.create_on_exp = r(common.sobject_types.size());
      w.delay = r(70);
      w.detect_distance = r(20);
      w.dirt_effect = r(9);
      w.distribution = r(5000) - 2500;
      w.expl_ground = r(2) == 0;
      w.explo_sound = r(common.sounds.size());
      w.fire_cone = r(10);
      w.gravity = r(2000) - 1000;
      w.hit_damage = r(20);
      w.laser_sight = r(5) == 0;
      w.launch_sound = r(common.sounds.size());
      w.leave_shell_delay = r(30);
      w.leave_shells = r(1) == 0;
      w.loading_time = r(70 * 3);
      w.loop_anim = r(10) == 0;
      w.loop_sound = false;
      w.mult_speed = r(2) ? 100 : 99 + r(5);
      w.obj_trail_delay = 10 + r(70);
      w.obj_trail_type = r(4) == 0 ? r(common.sobject_types.size()) : -1;
      w.parts = r(2) == 0 ? r(10) : 1;
      w.part_trail_delay = 10 + r(70);
      w.part_trail_obj = r(4) == 0 ? r(common.nobject_types.size()) : -1;
      w.part_trail_type = r(2);
      w.play_reload_sound = r(2) == 0;
      w.recoil = r(20);
      w.shadow = r(2) == 0;
      w.shot_type = r(5);
      w.speed = r(200);
      w.splinter_amount = r(5) == 0 ? r(10) : 0;
      w.splinter_colour = r(256);
      w.splinter_scatter = r(2);
      w.splinter_type = r(common.nobject_types.size());
      w.start_frame = r((uint32_t)common.small_sprites.count - 13);
      w.num_frames = r(5);
      w.time_to_explo = 50 + r(200);
      w.time_to_explo_v = 10 + r(50);
      w.worm_collide = r(3) > 0;
      w.worm_explode = r(3) > 0;
    }

    for (std::size_t idx = 0; idx < common.nobject_types.size(); ++idx) {
      auto& n = common.nobject_types[nobj_map[idx]];
      n.affect_by_explosions = r(5) == 0;
      n.blood_on_hit = r(5);
      n.blood_trail = r(10) == 0;
      n.blood_trail_delay = r(20) + 3;
      n.blow_away = r(10);
      n.bounce = r(90);
      n.color_bullets = 3 + r(250);
      n.create_on_exp = r(3) == 0 ? r(common.sobject_types.size()) : -1;
      n.detect_distance = r(20);
      n.dirt_effect = r(9);
      n.distribution = r(5000) - 2500;
      n.draw_on_map = r(20) == 0;
      n.expl_ground = r(4) > 0;
      n.gravity = r(2000) - 1000;
      n.hit_damage = r(10);
      n.leave_obj = r(5) == 0 ? r(common.sobject_types.size()) : -1;
      n.leave_obj_delay = 10 + r(80);
      n.start_frame = r((uint32_t)common.small_sprites.count - 13);
      n.num_frames = r(5);
      n.speed = r(150);
      n.splinter_amount = idx > 0 && r(5) == 0 ? r(10) : 0;
      n.splinter_colour = r(256);
      n.splinter_type = idx > 0 ? nobj_map[r(idx)] : 0;
      n.time_to_explo = 50 + r(70 * 3);
      n.time_to_explo_v = r(30);
      n.worm_destroy = r(3) == 0;
      n.worm_explode = r(2) == 0;
    }

    for (auto& s : common.sobject_types) {
      s.anim_delay = 1 + r(10);
      s.blow_away = r(2) == 0 ? r(10000) : 0;
      s.damage = r(30);
      s.detect_range = r(20);
      s.dirt_effect = r(9);
      s.flash = r(5);
      s.start_frame = r((uint32_t)common.large_sprites.count - 7);
      s.num_frames = r(7);
      s.start_sound = r(common.sounds.size());
      s.shake = r(10);
      s.shadow = r(2);
      s.num_sounds = 1;
    }
  }

  if (gfx->TestSdlKey(SDL_SCANCODE_LEFT) || gfx->TestControl(WormSettingsExtensions::kLeft) ||
      gfx->TestGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
    if (!gfx->cur_menu->OnLeftRight(common, -1)) ResetLeftRight();
  }
  if (gfx->TestSdlKey(SDL_SCANCODE_RIGHT) || gfx->TestControl(WormSettingsExtensions::kRight) ||
      gfx->TestGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
    if (!gfx->cur_menu->OnLeftRight(common, 1)) ResetLeftRight();
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_PAGEUP)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
    gfx->cur_menu->MovementPage(-1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_PAGEDOWN)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
    gfx->cur_menu->MovementPage(1);
  }

  if (selected_ >= 0) {
    // Transition to fade-out
    phase_ = Phase::kFadingOut;
    gfx->play_renderer.fade_value = 32;
    gfx->single_screen_renderer.fade_value = 32;
  }

  return true;
}

void MainMenuState::Draw() {
  gfx->DrawBasicMenu();
  gfx->DrawSpectatorInfo();

  Common& common = *gfx->common;

  if (gfx->cur_menu == &gfx->main_menu)
    gfx->settings_menu.Draw(common, gfx->play_renderer, true);
  else
    gfx->cur_menu->Draw(common, gfx->play_renderer, false);
}
