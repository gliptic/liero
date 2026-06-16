#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#if OPENLIERO_EMSCRIPTEN
#include <emscripten.h>
#endif

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "keys.hpp"
#include "metadata.hpp"
#include "mixer/player.hpp"
#include "reader.hpp"
#include "text.hpp"

#include "controller/controller.hpp"
#include "controller/localController.hpp"
#include "controller/replayController.hpp"
#include "profiling.hpp"

#include "gamePlayState.hpp"
#include "mainMenuState.hpp"
#include "netConnectState.hpp"
#include "onlineConnectState.hpp"

#include "gfx/macros.hpp"

#include "menu/arrayEnumBehavior.hpp"
#include "spectatorviewport.hpp"

// NOLINTNEXTLINE(bugprone-throwing-static-initialization, cert-err58-cpp) — global Gfx is the platform singleton; an exception here means program startup itself has failed.
Gfx gfx;

struct KeyBehavior : ItemBehavior {
  KeyBehavior(Common& common, uint32_t& key, uint32_t& key_ex, uint32_t& gamepad_key,
              uint32_t& input_device, bool extended = false)
      : common(common),
        key(key),
        key_ex(key_ex),
        gamepad_key(gamepad_key),
        input_device(input_device),
        extended(extended) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    if (input_device != WormSettingsExtensions::kInputKeyboard) {
      item.value = Gfx::GetGamepadKeyName(gamepad_key);
    } else {
      item.value = Gfx::GetKeyName(extended ? key_ex : key);
    }
    item.has_value = true;
  }

  Common& common;
  uint32_t& key;
  uint32_t& key_ex;
  uint32_t& gamepad_key;
  uint32_t& input_device;
  bool extended;
};

struct WormNameBehavior : ItemBehavior {
  WormNameBehavior(Common& common, WormSettings& ws) : common(common), ws(ws) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    item.value = ws.name;
    item.has_value = true;
  }

  Common& common;
  WormSettings& ws;
};

struct InputDeviceBehavior : ItemBehavior {
  struct GamepadOption {
    std::string name;
    std::string serial;
    std::string display_name;
    int joystick_idx;
  };

  InputDeviceBehavior(Common& common, WormSettings& ws) : common(common), ws(ws) {}

  static std::vector<GamepadOption> BuildOptions() {
    std::vector<GamepadOption> opts;
    // Count names to detect duplicates
    std::unordered_map<std::string, int> name_counts;
    for (auto& joystick : gfx.joysticks) {
      char const* n = SDL_GetGamepadName(joystick.sdl_gamepad);
      std::string const kName = n ? n : "Gamepad";
      name_counts[kName]++;
    }

    std::unordered_map<std::string, int> name_seen_so_far;
    for (int i = 0; std::cmp_less(i, gfx.joysticks.size()); ++i) {
      GamepadOption opt;
      char const* n = SDL_GetGamepadName(gfx.joysticks[i].sdl_gamepad);
      opt.name = n ? n : "Gamepad";
      char const* s = SDL_GetGamepadSerial(gfx.joysticks[i].sdl_gamepad);
      opt.serial = s ? s : "";
      opt.joystick_idx = i;

      // Disambiguate display name if duplicates exist
      if (name_counts[opt.name] > 1) {
        int const kIdx = ++name_seen_so_far[opt.name];
        std::string const kSuffix = " #" + ToString(kIdx);
        std::string const kBase = opt.name.substr(0, 20 - kSuffix.size());
        opt.display_name = kBase + kSuffix;
      } else {
        opt.display_name = opt.name.substr(0, 20);
      }
      opts.push_back(opt);
    }
    return opts;
  }

  int FindCurrentOption(std::vector<GamepadOption> const& opts) {
    if (ws.input_device == WormSettingsExtensions::kInputKeyboard) {
      return -1;
    }
    // Try serial match first
    if (!ws.gamepad_serial.empty()) {
      for (int i = 0; std::cmp_less(i, opts.size()); ++i) {
        if (opts[i].name == ws.gamepad_name && opts[i].serial == ws.gamepad_serial) {
          return i;
        }
      }
    }
    // Fall back to name match
    for (int i = 0; std::cmp_less(i, opts.size()); ++i) {
      if (opts[i].name == ws.gamepad_name) {
        return i;
      }
    }
    return -1;
  }

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    if (ws.input_device == WormSettingsExtensions::kInputKeyboard) {
      item.value = "Keyboard";
    } else {
      auto opts = BuildOptions();
      int const kCur = FindCurrentOption(opts);
      if (kCur >= 0) {
        item.value = opts[kCur].display_name;
      } else {
        std::string const kDisplay =
            ws.gamepad_name.empty() ? "Gamepad (none)" : ws.gamepad_name.substr(0, 20);
        item.value = kDisplay;
      }
    }
    item.has_value = true;
  }

  bool OnLeftRight(Menu& menu, MenuItem& /*item*/, int dir) override {
    g_sound_player->Play(common.sound_hook[dir > 0 ? SoundMenuMoveUp : SoundMenuMoveDown]);
    Cycle(menu, dir);
    return false;
  }

  int OnEnter(Menu& menu, MenuItem& /*item*/) override {
    g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
    Cycle(menu, 1);
    return -1;
  }

  void Cycle(Menu& menu, int dir) {
    auto opts = BuildOptions();
    // Options: -1 = keyboard, 0..N-1 = gamepads
    int const kCount = static_cast<int>(opts.size()) + 1;
    int cur = FindCurrentOption(opts) + 1;  // shift so keyboard=0, gamepads=1..N
    cur = ((cur + dir) % kCount + kCount) % kCount;

    if (cur == 0) {
      ws.input_device = WormSettingsExtensions::kInputKeyboard;
      ws.gamepad_name.clear();
      ws.gamepad_serial.clear();
    } else {
      auto& opt = opts[cur - 1];
      ws.input_device = 1;
      ws.gamepad_name = opt.name;
      ws.gamepad_serial = opt.serial;
    }

    menu.UpdateItems(common);
  }

  Common& common;
  WormSettings& ws;
};

struct ProfileSaveBehavior : ItemBehavior {
  ProfileSaveBehavior(Common& common, WormSettings& ws, bool save_as = false)
      : common(common), ws(ws), save_as(save_as) {}

  int OnEnter(Menu& menu, MenuItem& /*item*/) override {
    g_sound_player->Play(common.sound_hook[SoundMenuSelect]);

    if (!save_as) {
      // Save in-place always writes to the user dir, even when the
      // profile was loaded from shipped data. saveProfile retargets
      // profileNode so subsequent saves stay on the user copy.
      std::string const kLeaf = GetLeaf(ws.profile_node.FullPath());
      ws.SaveProfile(gfx.GetUserConfigNode() / "Profiles" / kLeaf);
    }
    // saveAs path is intercepted by MainMenuState

    menu.UpdateItems(common);
    return -1;
  }

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    if (!save_as) {
      item.visible = static_cast<bool>(ws.profile_node);
    }
  }

  Common& common;
  WormSettings& ws;
  bool save_as;
};

struct ProfileLoadedBehavior : ItemBehavior {
  ProfileLoadedBehavior(Common& common, WormSettings& ws) : common(common), ws(ws) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    if (ws.profile_node) {
      item.value = GetBasename(GetLeaf(ws.profile_node.FullPath()));
      item.visible = true;
    } else {
      item.value.clear();
      item.visible = false;
    }

    item.has_value = true;
  }

  Common& common;
  WormSettings& ws;
};

struct WeaponEnumBehavior : EnumBehavior {
  WeaponEnumBehavior(Common& common, uint32_t& v)
      : EnumBehavior(common, v, 1, static_cast<uint32_t>(common.weapons.size()),
                     /*broken_left_right=*/false) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    item.value = common.weapons[common.weap_order[v - 1]].name;
    item.has_value = true;
  }
};

Gfx::Gfx()
    : primary_renderer(&play_renderer),
      main_menu(53, 20),
      settings_menu(178, 20),
      player_menu(178, 20),
      hidden_menu(178, 20),

      key_buf_ptr(key_buf) {
  ClearKeys();

  sound_player = std::make_shared<NullSoundPlayer>();
}

void Gfx::Init() {
  SDL_HideCursor();
  last_frame = SDL_GetTicks();

  play_renderer.Init(320, 200);
  single_screen_renderer.Init(640, 400);
  // Gamepad init
  SDL_SetGamepadEventsEnabled(true);
  int num_gamepads = 0;
  SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&num_gamepads);
  joysticks.resize(num_gamepads);
  for (int i = 0; i < num_gamepads; ++i) {
    joysticks[i].sdl_gamepad = SDL_OpenGamepad(gamepad_ids[i]);
    joysticks[i].instance_id = gamepad_ids[i];
    joysticks[i].ClearState();
  }
  SDL_free(gamepad_ids);
}

void Gfx::SetVideoMode() {
  if (sdl_spectator_renderer) {
    SDL_DestroyRenderer(sdl_spectator_renderer);
    sdl_spectator_renderer = nullptr;
    // SDL_DestroyRenderer frees the renderer's textures; drop the dangling
    // world-texture handle so EnsureSpectatorWorldTexture rebuilds it lazily.
    sdl_spectator_world_texture = nullptr;
    sdl_spectator_world_texture_w = 0;
    sdl_spectator_world_texture_h = 0;
  }
  if (settings->spectator_window) {
    if (!sdl_spectator_window) {
      std::string const kSpectatorWindowTitle =
          std::string("Liero Spectator Window - ") + BuildVersion();
      sdl_spectator_window = SDL_CreateWindow(
          kSpectatorWindowTitle.c_str(), window_w, window_h,
          SDL_WINDOW_RESIZABLE | (spectator_fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    } else {
      SDL_SetWindowFullscreen(sdl_spectator_window, spectator_fullscreen);
    }
    sdl_spectator_renderer = SDL_CreateRenderer(sdl_spectator_window, nullptr);
    OnWindowResize(SDL_GetWindowID(sdl_spectator_window));
  } else {
    if (sdl_spectator_texture) {
      SDL_DestroyTexture(sdl_spectator_texture);
      sdl_spectator_texture = nullptr;
    }
    if (sdl_spectator_draw_surface) {
      SDL_DestroySurface(sdl_spectator_draw_surface);
      sdl_spectator_draw_surface = nullptr;
    }
    if (sdl_spectator_window) {
      SDL_DestroyWindow(sdl_spectator_window);
      sdl_spectator_window = nullptr;
    }
  }

  if (!sdl_window) {
    std::string const kWindowTitle = std::string("Liero ") + BuildVersion();
    sdl_window =
        SDL_CreateWindow(kWindowTitle.c_str(), window_w, window_h,
                         SDL_WINDOW_RESIZABLE | (settings->fullscreen ? SDL_WINDOW_FULLSCREEN : 0));

#ifndef __APPLE__
    std::string const kS = (GetConfigNode() / "Resources" / "icon.png").FullPath();
    SDL_Surface* icon = IMG_Load(kS.c_str());
    if (icon) {
      SDL_SetWindowIcon(sdl_window, icon);
      SDL_DestroySurface(icon);
    }
#endif
  } else {
    SDL_SetWindowFullscreen(sdl_window, settings->fullscreen);
  }
  if (sdl_renderer) {
    SDL_DestroyRenderer(sdl_renderer);
    sdl_renderer = nullptr;
  }
  // vertical sync is always disabled. Frame limiting is done manually below,
  // to keep the correct speed
  sdl_renderer = SDL_CreateRenderer(sdl_window, nullptr);
  OnWindowResize(SDL_GetWindowID(sdl_window));

  // Set the spectator window's icon after the main window has been initialized.
  // On Windows, this makes sure the icon in the stacked taskbar is the main icon.
  // On MacOS this is commented out, because it only allows one icon and the spectator icon
  // will override the main icon
#ifndef __APPLE__
  if (sdl_spectator_window) {
    std::string const kS = (GetConfigNode() / "Resources" / "spectator_icon.png").FullPath();
    SDL_Surface* spectator_icon = IMG_Load(kS.c_str());
    if (spectator_icon) {
      SDL_SetWindowIcon(sdl_spectator_window, spectator_icon);
      SDL_DestroySurface(spectator_icon);
    }
  }
#endif
}

void Gfx::OnWindowResize(uint32_t window_id) {
  if (window_id == SDL_GetWindowID(sdl_window)) {
    if (sdl_texture) {
      SDL_DestroyTexture(sdl_texture);
      sdl_texture = nullptr;
    }
    sdl_texture =
        SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                          double_res ? 640 : 320, double_res ? 400 : 200);

    if (sdl_draw_surface) {
      SDL_DestroySurface(sdl_draw_surface);
      sdl_draw_surface = nullptr;
    }
    sdl_draw_surface =
        SDL_CreateSurface(double_res ? 640 : 320, double_res ? 400 : 200, SDL_PIXELFORMAT_ARGB8888);
    // linear for that old-school chunky look, but consider adding a user
    // option for this
    SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_LINEAR);
    SDL_SetRenderLogicalPresentation(sdl_renderer, double_res ? 640 : 320, double_res ? 400 : 200,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
  } else {
    if (sdl_spectator_texture) {
      SDL_DestroyTexture(sdl_spectator_texture);
      sdl_spectator_texture = nullptr;
    }
    if (sdl_spectator_draw_surface) {
      SDL_DestroySurface(sdl_spectator_draw_surface);
      sdl_spectator_draw_surface = nullptr;
    }

    if (settings->spectator_window) {
      int window_w = 0;
      int window_h = 0;
      SDL_GetWindowSize(sdl_spectator_window, &window_w, &window_h);
      // Cap the internal render resolution so the world pass, its memsets and
      // texture uploads are bounded by a constant on 4K-class windows.
      // SDL_SetRenderLogicalPresentation (below) upscales the capped surface
      // back to the physical window. A no-op when window_h <= cap.
      CappedRenderResolution const kCap =
          ComputeCappedRenderResolution(window_w, window_h, settings->max_spectator_render_height);
      int const kW = kCap.w;
      int const kH = kCap.h;
      sdl_spectator_texture = SDL_CreateTexture(sdl_spectator_renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, kW, kH);
      // Blend so the GPU HUD overlay composites over the world; harmless for
      // the CPU present path, whose frames are fully opaque.
      SDL_SetTextureBlendMode(sdl_spectator_texture, SDL_BLENDMODE_BLEND);
      // Point-sample the HUD overlay so the small pixel font stays crisp when
      // SDL upscales the capped surface to the physical window. The world
      // texture keeps linear sampling for a smooth downscaled overview; only
      // the HUD needs nearest to avoid blurring the pixel font.
      SDL_SetTextureScaleMode(sdl_spectator_texture, SDL_SCALEMODE_NEAREST);
      sdl_spectator_draw_surface = SDL_CreateSurface(kW, kH, SDL_PIXELFORMAT_ARGB8888);
      SDL_SetRenderLogicalPresentation(sdl_spectator_renderer, kW, kH,
                                       SDL_LOGICAL_PRESENTATION_LETTERBOX);
      // Only resize the renderer when it isn't being used as primary renderer
      // for the main window (i.e. during single-screen replay).
      if (primary_renderer != &single_screen_renderer) {
        single_screen_renderer.SetRenderResolution(kW, kH);
      }
    }
  }
}

void Gfx::LoadMenus() {
  hidden_menu.AddItem(MenuItem(48, 7, "FULLSCREEN (F11)", HiddenMenu::kFullscreen));
  hidden_menu.AddItem(MenuItem(48, 7, "MODERN COLORS (F10)", HiddenMenu::kColorMode));
  hidden_menu.AddItem(MenuItem(48, 7, "DOUBLE SIZE", HiddenMenu::kDoubleRes));
  hidden_menu.AddItem(MenuItem(48, 7, "POWERLEVEL PALETTES", HiddenMenu::kLoadPowerLevels));
  hidden_menu.AddItem(MenuItem(48, 7, "SHADOWS", HiddenMenu::kShadows));
  hidden_menu.AddItem(MenuItem(48, 7, "AUTO-RECORD REPLAYS", HiddenMenu::kRecordReplays));
  hidden_menu.AddItem(MenuItem(48, 7, "AI FRAMES", HiddenMenu::kAiFrames));
  hidden_menu.AddItem(MenuItem(48, 7, "AI MUTATIONS", HiddenMenu::kAiMutations));
  hidden_menu.AddItem(MenuItem(48, 7, "AI PARALLELS", HiddenMenu::kAiParallels));
  hidden_menu.AddItem(MenuItem(48, 7, "AI TRACES", HiddenMenu::kAiTraces));
  hidden_menu.AddItem(MenuItem(48, 7, "PALETTE", HiddenMenu::kPaletteSelect));
  hidden_menu.AddItem(MenuItem(48, 7, "BOT WEAPONS", HiddenMenu::kSelectBotWeapons));
  hidden_menu.AddItem(MenuItem(48, 7, "SEE SPAWN POINT", HiddenMenu::kAllowViewingSpawnPoint));
  hidden_menu.AddItem(MenuItem(48, 7, "SINGLE SCREEN REPLAY", HiddenMenu::kSingleScreenReplay));
  hidden_menu.AddItem(MenuItem(48, 7, "SPECTATOR WINDOW", HiddenMenu::kSpectatorWindow));
  hidden_menu.AddItem(
      MenuItem(48, 7, "MAX SPECTATOR RES (H)", HiddenMenu::kMaxSpectatorRenderHeight));

  player_menu.AddItem(MenuItem(3, 7, "PROFILE LOADED", PlayerMenu::kPlLoadedProfile));
  player_menu.AddItem(MenuItem(3, 7, "SAVE PROFILE", PlayerMenu::kPlSaveProfile));
  player_menu.AddItem(MenuItem(3, 7, "SAVE PROFILE AS...", PlayerMenu::kPlSaveProfileAs));
  player_menu.AddItem(MenuItem(3, 7, "LOAD PROFILE", PlayerMenu::kPlLoadProfile));
  player_menu.AddItem(MenuItem(48, 7, "NAME", PlayerMenu::kPlName));
  player_menu.AddItem(MenuItem(48, 7, "HEALTH", PlayerMenu::kPlHealth));
  player_menu.AddItem(MenuItem(48, 7, "Red", PlayerMenu::kPlRed));
  player_menu.AddItem(MenuItem(48, 7, "Green", PlayerMenu::kPlGreen));
  player_menu.AddItem(MenuItem(48, 7, "Blue", PlayerMenu::kPlBlue));
  player_menu.AddItem(MenuItem(48, 7, "INPUT", PlayerMenu::kPlInput));
  player_menu.AddItem(MenuItem(48, 7, "AIM UP", PlayerMenu::kPlUp));
  player_menu.AddItem(MenuItem(48, 7, "AIM DOWN", PlayerMenu::kPlDown));
  player_menu.AddItem(MenuItem(48, 7, "MOVE LEFT", PlayerMenu::kPlLeft));
  player_menu.AddItem(MenuItem(48, 7, "MOVE RIGHT", PlayerMenu::kPlRight));
  player_menu.AddItem(MenuItem(48, 7, "FIRE", PlayerMenu::kPlFire));
  player_menu.AddItem(MenuItem(48, 7, "CHANGE", PlayerMenu::kPlChange));
  player_menu.AddItem(MenuItem(48, 7, "JUMP", PlayerMenu::kPlJump));
  player_menu.AddItem(MenuItem(48, 7, "DIG", PlayerMenu::kPlDig));

  for (int i = 0; i < 5; ++i) {
    player_menu.AddItem(MenuItem(48, 7, std::string("WEAPON ") + static_cast<char>(i + '1'),
                                 PlayerMenu::kPlWeap0 + i));
  }

  player_menu.AddItem(MenuItem(48, 7, "CONTROLLER", PlayerMenu::kPlController));

  settings_menu.AddItem(MenuItem(48, 7, "GAME MODE", SettingsMenu::kSiGameMode));
  settings_menu.AddItem(MenuItem(48, 7, "TIME TO LOSE", SettingsMenu::kSiTimeToLose));
  settings_menu.AddItem(MenuItem(48, 7, "TIME TO WIN", SettingsMenu::kSiTimeToWin));
  settings_menu.AddItem(MenuItem(48, 7, "ZONE TIMEOUT", SettingsMenu::kSiZoneTimeout));
  settings_menu.AddItem(MenuItem(48, 7, "FLAGS TO WIN", SettingsMenu::kSiFlagsToWin));
  settings_menu.AddItem(MenuItem(48, 7, "LIVES", SettingsMenu::kSiLives));
  settings_menu.AddItem(MenuItem(48, 7, "LEVEL", SettingsMenu::kSiLevel));
  settings_menu.AddItem(MenuItem(48, 7, "MAP WIDTH", SettingsMenu::kSiRandomMapWidth));
  settings_menu.AddItem(MenuItem(48, 7, "MAP HEIGHT", SettingsMenu::kSiRandomMapHeight));
  settings_menu.AddItem(MenuItem(48, 7, "LOADING TIMES", SettingsMenu::kSiLoadingTimes));
  settings_menu.AddItem(MenuItem(48, 7, "WEAPON OPTIONS", SettingsMenu::kSiWeaponOptions));
  settings_menu.AddItem(MenuItem(48, 7, "MAX BONUSES", SettingsMenu::kSiMaxBonuses));
  settings_menu.AddItem(MenuItem(48, 7, "NAMES ON BONUSES", SettingsMenu::kSiNamesOnBonuses));
  settings_menu.AddItem(MenuItem(48, 7, "MAP", SettingsMenu::kSiMap));
  settings_menu.AddItem(MenuItem(48, 7, "AMOUNT OF BLOOD", SettingsMenu::kSiAmountOfBlood));
  settings_menu.AddItem(MenuItem(48, 7, "LOAD+CHANGE", SettingsMenu::kLoadChange));
  settings_menu.AddItem(MenuItem(48, 7, "REGENERATE LEVEL", SettingsMenu::kSiRegenerateLevel));
  settings_menu.AddItem(MenuItem(48, 7, "SAVE SETUP AS...", SettingsMenu::kSaveOptions));
  settings_menu.AddItem(MenuItem(48, 7, "LOAD SETUP", SettingsMenu::kLoadOptions));

  main_menu.AddItem(
      MenuItem(10, 10, "", MainMenu::kMaResumeGame));  // string set in MainMenuState::enter()
  main_menu.AddItem(
      MenuItem(10, 10, "", MainMenu::kMaNewGame));  // string set in MainMenuState::enter()
  main_menu.AddItem(MenuItem(48, 48, "HOST LAN GAME", MainMenu::kMaHostGame));
  main_menu.AddItem(MenuItem(48, 48, "JOIN LAN GAME", MainMenu::kMaJoinGame));
  main_menu.AddItem(MenuItem(48, 48, "HOST ONLINE", MainMenu::kMaHostOnline));
  main_menu.AddItem(MenuItem(48, 48, "JOIN ONLINE", MainMenu::kMaJoinOnline));
  main_menu.AddItem(MenuItem(48, 48, "OPTIONS (F2)", MainMenu::kMaAdvanced));
  main_menu.AddItem(MenuItem(48, 48, "REPLAYS (F3)", MainMenu::kMaReplays));
  main_menu.AddItem(MenuItem(48, 48, "TC", MainMenu::kMaTc));
  main_menu.AddItem(MenuItem(6, 6, "QUIT TO OS", MainMenu::kMaQuit));
  main_menu.AddItem(MenuItem::Space());
  main_menu.AddItem(MenuItem(48, 48, "LEFT PLAYER (F5)", MainMenu::kMaPlayer1Settings));
  main_menu.AddItem(MenuItem(48, 48, "RIGHT PLAYER (F6)", MainMenu::kMaPlayer2Settings));
  main_menu.AddItem(MenuItem(48, 48, "NETWORK PLAYER (F9)", MainMenu::kMaNetPlayerSettings));
  main_menu.AddItem(MenuItem(48, 48, "MATCH SETUP (F7)", MainMenu::kMaSettings));

  settings_menu.value_offset_x = 100;
  player_menu.value_offset_x = 95;
  hidden_menu.value_offset_x = 120;
}

void Gfx::SetSpectatorFullscreen(bool new_fullscreen) {
  if (new_fullscreen == spectator_fullscreen) {
    return;
  }
  spectator_fullscreen = new_fullscreen;

  if (!spectator_fullscreen) {
    if (double_res) {
      window_w = 640;
      window_h = 400;
    } else {
      window_w = 320;
      window_h = 200;
    }
  }
  SetVideoMode();
}

void Gfx::SetColorMode(ColorMode new_mode) {
  play_renderer.mode = new_mode;
  single_screen_renderer.mode = new_mode;
  settings->modern_colors = new_mode == ColorMode::kModern;

  // Item value strings are only rebuilt on menu events, and the colour
  // picker renders its numbers in mode-dependent units — refresh the open
  // menu so they don't go stale.
  if (cur_menu && common) {
    cur_menu->UpdateItems(*common);
  }
}

void Gfx::SetFullscreen(bool new_fullscreen) {
  if (new_fullscreen == settings->fullscreen) {
    return;
  }
  settings->fullscreen = new_fullscreen;

  // fullscreen will automatically set window size
  if (!settings->fullscreen) {
    if (double_res) {
      window_w = 640;
      window_h = 400;
    } else {
      window_w = 320;
      window_h = 200;
    }
  }
  SetVideoMode();
  hidden_menu.UpdateItems(*common);
}

void Gfx::SetDoubleRes(bool new_double_res) {
  if (new_double_res == double_res) {
    return;
  }
  double_res = new_double_res;

  if (!new_double_res) {
    window_w = 320;
    window_h = 200;
  } else {
    window_w = 640;
    window_h = 400;
  }
  SetVideoMode();
  hidden_menu.UpdateItems(*common);
}

void Gfx::ProcessEvent(SDL_Event& ev, Controller* controller) {
  switch (ev.type) {
    case SDL_EVENT_KEY_DOWN: {
      SDL_Scancode const kS = ev.key.scancode;

      if (key_buf_ptr < key_buf + 32) {
        *key_buf_ptr++ = ev.key.scancode;
      }

      uint32_t const kDosScan = SDLToDOSKey(ev.key.scancode);
      if (kDosScan) {
        dos_keys[kDosScan] = true;
        if (controller && !ev.key.repeat) {
          controller->OnKey(kDosScan, /*state=*/true);
        }
      }

      if (kS == SDL_SCANCODE_F11) {
        if (SDL_GetWindowFromID(ev.key.windowID) == sdl_window) {
          SetFullscreen(!settings->fullscreen);
        } else {
          SetSpectatorFullscreen(!spectator_fullscreen);
        }
      }

      if (kS == SDL_SCANCODE_F10) {
        SetColorMode(play_renderer.mode == ColorMode::kModern ? ColorMode::kClassic
                                                              : ColorMode::kModern);
      }

      if (kS == SDL_SCANCODE_F4 && (ev.key.mod & SDL_KMOD_ALT)) {
        running = false;
      }
    } break;

    case SDL_EVENT_KEY_UP: {
      SDL_Scancode const kS = ev.key.scancode;

      uint32_t const kDosScan = SDLToDOSKey(kS);
      if (kDosScan) {
        dos_keys[kDosScan] = false;
        if (controller) {
          controller->OnKey(kDosScan, /*state=*/false);
        }
      }
    } break;

    case SDL_EVENT_WINDOW_RESIZED: {
      OnWindowResize(ev.window.windowID);
    } break;

    case SDL_EVENT_QUIT: {
      running = false;
    } break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      int const kGpIdx = FindGamepadIndex(ev.gaxis.which);
      if (kGpIdx < 0) {
        break;
      }
      Joystick& js = joysticks[kGpIdx];
      int const kAxis = ev.gaxis.axis;

      bool const kPosState = (ev.gaxis.value > kJoyAxisThreshold);
      bool const kNegState = (ev.gaxis.value < -kJoyAxisThreshold);

      int const kPosIdx = kAxis * 2;
      int const kNegIdx = kAxis * 2 + 1;

      if (kPosState != js.axis_button_state[kPosIdx]) {
        js.axis_button_state[kPosIdx] = kPosState;
        if (kPosState) {
          js.axis_pressed[kPosIdx] = true;
        }
        DispatchGamepadInput(kGpIdx, WormSettingsExtensions::GamepadAxisPositive(kAxis), kPosState,
                             controller);
      }
      if (kNegState != js.axis_button_state[kNegIdx]) {
        js.axis_button_state[kNegIdx] = kNegState;
        if (kNegState) {
          js.axis_pressed[kNegIdx] = true;
        }
        DispatchGamepadInput(kGpIdx, WormSettingsExtensions::GamepadAxisNegative(kAxis), kNegState,
                             controller);
      }
    } break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      int const kGpIdx = FindGamepadIndex(ev.gbutton.which);
      if (kGpIdx < 0) {
        break;
      }
      Joystick& js = joysticks[kGpIdx];
      int const kBtn = ev.gbutton.button;
      bool const kState = ev.gbutton.down;
      js.btn_state[kBtn] = kState;
      if (kState) {
        js.btn_pressed[kBtn] = true;
      }
      DispatchGamepadInput(kGpIdx, static_cast<uint32_t>(kBtn), kState, controller);
    } break;

    case SDL_EVENT_GAMEPAD_ADDED: {
      SDL_JoystickID const kId = ev.gdevice.which;
      // Only track up to 2 gamepads
      if (joysticks.size() < 2) {
        Joystick js;
        js.sdl_gamepad = SDL_OpenGamepad(kId);
        js.instance_id = kId;
        js.ClearState();
        joysticks.push_back(js);
      }
    } break;

    case SDL_EVENT_GAMEPAD_REMOVED: {
      SDL_JoystickID const kId = ev.gdevice.which;
      for (auto it = joysticks.begin(); it != joysticks.end(); ++it) {
        if (it->instance_id == kId) {
          SDL_CloseGamepad(it->sdl_gamepad);
          joysticks.erase(it);
          break;
        }
      }
    } break;

    default:
      break;
  }
}

void Gfx::Process(Controller* controller) {
  SDL_Event ev;
  key_buf_ptr = key_buf;
  while (SDL_PollEvent(&ev)) {
    ProcessEvent(ev, controller);
  }
}

int Gfx::FindGamepadIndex(SDL_JoystickID id) {
  for (int i = 0; std::cmp_less(i, joysticks.size()); ++i) {
    if (joysticks[i].instance_id == id) {
      return i;
    }
  }
  return -1;
}

int Gfx::FindGamepadForPlayer(int player_idx) {
  if (!settings || player_idx < 0 || player_idx >= 2) {
    return -1;
  }
  WormSettings const& ws = *settings->worm_settings[player_idx];
  if (ws.input_device == WormSettingsExtensions::kInputKeyboard) {
    return -1;
  }
  if (ws.gamepad_name.empty()) {
    return -1;
  }

  // Collect indices of gamepads matching by name
  std::vector<int> candidates;
  for (int i = 0; std::cmp_less(i, joysticks.size()); ++i) {
    char const* n = SDL_GetGamepadName(joysticks[i].sdl_gamepad);
    if (!n || ws.gamepad_name != n) {
      continue;
    }

    // Exact serial match is best
    if (!ws.gamepad_serial.empty()) {
      char const* s = SDL_GetGamepadSerial(joysticks[i].sdl_gamepad);
      if (s && ws.gamepad_serial == s) {
        return i;
      }
    }
    candidates.push_back(i);
  }

  if (candidates.empty()) {
    return -1;
  }

  // No serial match — resolve by position among same-name candidates.
  // If the other player also wants a gamepad with the same name,
  // give the first candidate to player 0 and the second to player 1.
  int const kOtherPlayer = 1 - player_idx;
  WormSettings const& other_ws = *settings->worm_settings[kOtherPlayer];
  if (candidates.size() >= 2 && other_ws.input_device != WormSettingsExtensions::kInputKeyboard &&
      other_ws.gamepad_name == ws.gamepad_name) {
    // Both players want same-named gamepad — split by player index
    return candidates[player_idx < kOtherPlayer ? 0 : 1];
  }

  return candidates[0];
}

void Gfx::DispatchGamepadInput(int gp_idx, uint32_t gamepad_key, bool state,
                               Controller* controller) {
  if (gp_idx < 0 || gp_idx >= 2) {
    return;
  }

  // Start button acts as ESC for menu access
  if (gamepad_key == static_cast<uint32_t>(SDL_GAMEPAD_BUTTON_START) && state) {
    dos_keys[kDkEscape] = true;
    if (controller) {
      controller->OnKey(kDkEscape, /*state=*/true);
    }
    return;
  }

  // Dispatch to the controller for the player who has this gamepad assigned
  if (controller) {
    for (int p = 0; p < 2; ++p) {
      WormSettings const& ws = *settings->worm_settings[p];
      if (ws.input_device == WormSettingsExtensions::kInputKeyboard) {
        continue;
      }
      if (FindGamepadForPlayer(p) != gp_idx) {
        continue;
      }

      for (int c = 0; c < WormSettings::kMaxControlEx; ++c) {
        if (ws.gamepad_controls[c] == gamepad_key) {
          controller->OnKey(GamepadControlToExKey(p, c), state);
        }
      }
    }
  }
}

// Static method: the body uses only Common-level data (Texts::key_names).
std::string Gfx::GetKeyName(uint32_t key) {
  if (key < kMaxDosKey) {
    return Texts::key_names[key];
  }
  if (key >= kJoyKeysStart) {
    key -= kJoyKeysStart;
    int const kJoyNum = key / kMaxJoyButtons;
    key -= kJoyNum * kMaxJoyButtons;
    return "J" + ToString(kJoyNum) + "_" + ToString(key);
  }

  return "";
}

std::string Gfx::GetGamepadKeyName(uint32_t gamepad_key) {
  if (WormSettingsExtensions::IsGamepadAxis(gamepad_key)) {
    int const kAxis = (gamepad_key - WormSettingsExtensions::kGamepadAxisBase) / 2;
    bool const kNegative = (gamepad_key - WormSettingsExtensions::kGamepadAxisBase) % 2;
    static char const* const kAxisNames[] = {"LX", "LY", "RX", "RY", "LT", "RT"};
    std::string const kName = (kAxis < 6) ? kAxisNames[kAxis] : "A" + ToString(kAxis);
    return kName + (kNegative ? "-" : "+");
  }

  static char const* const kButtonNames[] = {"A",  "B",  "X",  "Y",  "Back", "Guide", "Start", "LS",
                                             "RS", "LB", "RB", "Up", "Down", "Left",  "Right"};

  if (gamepad_key < 15) {
    return kButtonNames[gamepad_key];
  }

  return "Btn" + ToString(gamepad_key);
}

void Gfx::ClearKeys() {
  std::memset(dos_keys, 0, sizeof(dos_keys));
  ex_keys.clear();
  for (auto& js : joysticks) {
    js.ClearState();
  }
}

bool Gfx::TestControlOnce(int control) {
  // Check keyboard bindings for all player profiles (left, right, network)
  // NOLINTNEXTLINE(readability-use-anyofallof) — loop body has more than the predicate check; rewriting as std::any_of/all_of would be less readable here.
  for (auto& worm_setting : settings->worm_settings) {
    if (worm_setting->input_device != WormSettingsExtensions::kInputKeyboard) {
      continue;
    }
    uint32_t const kEy = Settings::kExtensions ? worm_setting->controls_ex[control]
                                               : worm_setting->controls[control];
    if (TestAnyKeyOnce(kEy)) {
      return true;
    }
  }
  return false;
}

bool Gfx::TestGamepadButtonOnce(int button) {
  for (auto& joystick : joysticks) {
    if (joystick.btn_pressed[button]) {
      joystick.btn_pressed[button] = false;
      return true;
    }
  }
  return false;
}

bool Gfx::TestGamepadButton(int button) {
  // NOLINTNEXTLINE(readability-use-anyofallof) — loop body has more than the predicate check; rewriting as std::any_of/all_of would be less readable here.
  for (auto& joystick : joysticks) {
    if (joystick.btn_state[button]) {
      return true;
    }
  }
  return false;
}

// Map DPad button to left stick axis index (-1 if not a directional button)
static int DpadToAxisIndex(int dpad_button) {
  switch (dpad_button) {
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return 0;  // LEFTX positive
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return 1;  // LEFTX negative
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return 2;  // LEFTY positive
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return 3;  // LEFTY negative
    default:
      return -1;
  }
}

bool Gfx::TestGamepadDirOnce(int dpad_button) {
  int const kAxisIdx = DpadToAxisIndex(dpad_button);
  for (auto& joystick : joysticks) {
    if (joystick.btn_pressed[dpad_button]) {
      joystick.btn_pressed[dpad_button] = false;
      if (kAxisIdx >= 0) {
        joystick.axis_pressed[kAxisIdx] = false;
      }
      return true;
    }
    if (kAxisIdx >= 0 && joystick.axis_pressed[kAxisIdx]) {
      joystick.axis_pressed[kAxisIdx] = false;
      joystick.btn_pressed[dpad_button] = false;
      return true;
    }
  }
  return false;
}

bool Gfx::TestGamepadDir(int dpad_button) {
  int const kAxisIdx = DpadToAxisIndex(dpad_button);
  // NOLINTNEXTLINE(readability-use-anyofallof) — loop body has more than the predicate check; rewriting as std::any_of/all_of would be less readable here.
  for (auto& joystick : joysticks) {
    if (joystick.btn_state[dpad_button]) {
      return true;
    }
    if (kAxisIdx >= 0 && joystick.axis_button_state[kAxisIdx]) {
      return true;
    }
  }
  return false;
}

bool Gfx::TestControl(int control) {
  // Check keyboard bindings for all player profiles (left, right, network)
  // NOLINTNEXTLINE(readability-use-anyofallof) — loop body has more than the predicate check; rewriting as std::any_of/all_of would be less readable here.
  for (auto& worm_setting : settings->worm_settings) {
    if (worm_setting->input_device != WormSettingsExtensions::kInputKeyboard) {
      continue;
    }
    uint32_t const kEy = Settings::kExtensions ? worm_setting->controls_ex[control]
                                               : worm_setting->controls[control];
    if (TestAnyKey(kEy)) {
      return true;
    }
  }
  return false;
}

void Gfx::ReleaseControl(int control) {
  for (auto& worm_setting : settings->worm_settings) {
    uint32_t const kEy = Settings::kExtensions ? worm_setting->controls_ex[control]
                                               : worm_setting->controls[control];
    ReleaseAnyKey(kEy);
  }
}

void Gfx::UpdateMenuPalettes(bool quitting) {
  if (play_renderer.fade_value < 32 && !quitting) {
    ++play_renderer.fade_value;
  }
  if (single_screen_renderer.fade_value < 32 && !quitting) {
    ++single_screen_renderer.fade_value;
  }

  ++menu_cycles;
  play_renderer.pal = play_renderer.Origpal();
  play_renderer.pal.RotateFrom(play_renderer.Origpal(), 168, 174, menu_cycles);
  play_renderer.pal.SetWormColours(*settings, play_renderer.mode);
  if (cur_menu == &player_menu &&
      player_menu.ws == settings->worm_settings[Settings::kNetworkPlayerIdx]) {
    play_renderer.pal.SetWormColour(0, *player_menu.ws, play_renderer.mode);
  }
  // The fade applies at composition (ScaleDraw); menus draw through pal32,
  // so every rebuild must end with a repack.
  play_renderer.UpdatePal32();
  single_screen_renderer.pal = single_screen_renderer.Origpal();
  single_screen_renderer.pal.RotateFrom(single_screen_renderer.Origpal(), 168, 174, menu_cycles);
  single_screen_renderer.pal.SetWormColours(*settings, single_screen_renderer.mode);
  if (cur_menu == &player_menu &&
      player_menu.ws == settings->worm_settings[Settings::kNetworkPlayerIdx]) {
    single_screen_renderer.pal.SetWormColour(0, *player_menu.ws, single_screen_renderer.mode);
  }
  single_screen_renderer.UpdatePal32();
}

void Gfx::Draw(SDL_Surface& surface, SDL_Texture& texture, SDL_Renderer& sdl_renderer,
               Renderer& renderer) {
  ZoneScopedN("Gfx::Draw");
  Rect update_rect;
  int offset_x = 0;
  int offset_y = 0;
  int const kMag = FitScreen(surface.w, surface.h, renderer.render_res_x, renderer.render_res_y,
                             offset_x, offset_y);

  Rect const kNewRect(offset_x, offset_y, renderer.render_res_x * kMag,
                      renderer.render_res_y * kMag);

  if (kMag != prev_mag) {
    // Clear background if magnification is decreased to
    // avoid leftovers.
    SDL_FillSurfaceRect(&surface, nullptr, 0);
    update_rect = last_update_rect | kNewRect;
  } else {
    update_rect = kNewRect;
  }
  prev_mag = kMag;

  std::size_t const kDestPitch = surface.pitch;
  std::size_t const kSrcPitch = renderer.bmp.pitch;

  // Both surfaces are fixed SDL_PIXELFORMAT_ARGB8888; the back buffer is
  // already ARGB, so composition is a (faded) magnifying copy.
  uint8_t* dest = static_cast<uint8_t*>(surface.pixels) + offset_y * kDestPitch +
                  static_cast<std::size_t>(offset_x) * 4;

  ScaleDraw(renderer.bmp.pixels, renderer.render_res_x, renderer.render_res_y, kSrcPitch, dest,
            kDestPitch, kMag, renderer.fade_value);

  SDL_UpdateTexture(&texture, nullptr, surface.pixels, surface.w * 4);
  SDL_RenderClear(&sdl_renderer);
  SDL_RenderTexture(&sdl_renderer, &texture, nullptr, nullptr);
  SDL_RenderPresent(&sdl_renderer);

  last_update_rect = update_rect;
}

bool Gfx::SpectatorGpuComposite() const {
  // The GPU composite is spectator-window-only. It needs a live spectator
  // renderer + overlay texture, and must not fire when the single-screen
  // renderer is currently the main window's primary renderer (single-screen
  // replay) — that frame is presented via the CPU Draw path.
  return settings->spectator_window && sdl_spectator_renderer != nullptr &&
         sdl_spectator_texture != nullptr && primary_renderer != &single_screen_renderer;
}

void Gfx::EnsureSpectatorWorldTexture(int need_w, int need_h) {
  if (!sdl_spectator_renderer || need_w <= 0 || need_h <= 0) {
    return;
  }
  if (sdl_spectator_world_texture && sdl_spectator_world_texture_w >= need_w &&
      sdl_spectator_world_texture_h >= need_h) {
    return;  // already big enough — allocated once per level, not per frame
  }
  if (sdl_spectator_world_texture) {
    SDL_DestroyTexture(sdl_spectator_world_texture);
    sdl_spectator_world_texture = nullptr;
  }
  sdl_spectator_world_texture = SDL_CreateTexture(sdl_spectator_renderer, SDL_PIXELFORMAT_ARGB8888,
                                                  SDL_TEXTUREACCESS_STREAMING, need_w, need_h);
  sdl_spectator_world_texture_w = need_w;
  sdl_spectator_world_texture_h = need_h;
  if (sdl_spectator_world_texture) {
    // Linear scaling is the whole point: the world is downscaled to the
    // letterboxed dest rect on the GPU. Opaque base layer — no blend.
    SDL_SetTextureScaleMode(sdl_spectator_world_texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(sdl_spectator_world_texture, SDL_BLENDMODE_NONE);
  }
}

void Gfx::DrawSpectatorGpu(Renderer& renderer) {
  ZoneScopedN("Gfx::DrawSpectatorGpu");
  EnsureSpectatorWorldTexture(renderer.gpu_world_max_w, renderer.gpu_world_max_h);
  if (!sdl_spectator_world_texture) {
    // Allocation failed; present via the CPU path so the window isn't black.
    Draw(*sdl_spectator_draw_surface, *sdl_spectator_texture, *sdl_spectator_renderer, renderer);
    spectator_prev_present_gpu = false;
    return;
  }

  Bitmap const& world = *renderer.gpu_world_src;
  // Upload only the used sub-rect; the scratch content is anchored at (0,0).
  SDL_Rect const kUsed{
      .x = 0, .y = 0, .w = renderer.gpu_world_used_w, .h = renderer.gpu_world_used_h};
  SDL_UpdateTexture(sdl_spectator_world_texture, &kUsed, world.pixels,
                    static_cast<int>(world.pitch * sizeof(uint32_t)));
  // HUD overlay: transparent background, opaque HUD pixels. Upload only the
  // dirty bands — except the first GPU frame after a resolution change or a
  // CPU present, which must refresh the whole overlay since the texture's
  // untouched rows would otherwise hold stale content.
  int const kHudPitchBytes = static_cast<int>(renderer.bmp.pitch * sizeof(uint32_t));
  bool const kFullHud = renderer.hud_overlay_full_refresh || !spectator_prev_present_gpu;
  if (kFullHud) {
    SDL_UpdateTexture(sdl_spectator_texture, nullptr, renderer.bmp.pixels, kHudPitchBytes);
  } else {
    for (int i = 0; i < renderer.hud_overlay_band_count; ++i) {
      int const kY = renderer.hud_overlay_band_y[i];
      SDL_Rect const kBand{
          .x = 0, .y = kY, .w = renderer.render_res_x, .h = renderer.hud_overlay_band_h[i]};
      uint32_t const* const kRows =
          renderer.bmp.pixels + static_cast<std::size_t>(kY) * renderer.bmp.pitch;
      SDL_UpdateTexture(sdl_spectator_texture, &kBand, kRows, kHudPitchBytes);
    }
  }

  // Replicate ScaleDraw's per-channel fade ((v*fade)>>5, fade 0..32) via RGB
  // texture modulation so the spectator fade-in survives; alpha is left intact
  // so the HUD overlay still blends.
  Uint8 const kMod =
      renderer.fade_value >= 32 ? 255 : static_cast<Uint8>((renderer.fade_value * 255) >> 5);
  SDL_SetTextureColorMod(sdl_spectator_world_texture, kMod, kMod, kMod);
  SDL_SetTextureColorMod(sdl_spectator_texture, kMod, kMod, kMod);

  SDL_SetRenderDrawColor(sdl_spectator_renderer, 0, 0, 0, 255);
  SDL_RenderClear(sdl_spectator_renderer);  // opaque-black letterbox bars
  SDL_FRect const kSrc{.x = 0.0F,
                       .y = 0.0F,
                       .w = static_cast<float>(renderer.gpu_world_used_w),
                       .h = static_cast<float>(renderer.gpu_world_used_h)};
  SDL_FRect const kDst{.x = static_cast<float>(renderer.gpu_world_dst_x),
                       .y = static_cast<float>(renderer.gpu_world_dst_y),
                       .w = static_cast<float>(renderer.gpu_world_dst_w),
                       .h = static_cast<float>(renderer.gpu_world_dst_h)};
  SDL_RenderTexture(sdl_spectator_renderer, sdl_spectator_world_texture, &kSrc, &kDst);
  SDL_RenderTexture(sdl_spectator_renderer, sdl_spectator_texture, nullptr, nullptr);
  SDL_RenderPresent(sdl_spectator_renderer);

  // sdl_spectator_texture is shared with the CPU present path (menus, pause,
  // weapon select), which applies fade itself via ScaleDraw and expects a
  // neutral texture. Restore the modulation so this frame's fade value doesn't
  // leak — at fade 0 it would otherwise leave the texture multiplying by black
  // and the CPU path would render an all-black spectator window until a resize
  // recreated the texture.
  SDL_SetTextureColorMod(sdl_spectator_world_texture, 255, 255, 255);
  SDL_SetTextureColorMod(sdl_spectator_texture, 255, 255, 255);

  // Record that the overlay texture was last written by the GPU path, so the
  // next GPU frame may upload just its dirty bands.
  spectator_prev_present_gpu = true;
}

void Gfx::Flip() {
  ZoneScopedN("Gfx::Flip");
  // draw into the play window. This uses either the normal split screen renderer
  // or the single screen renderer if this is a replay and single screen replay
  // is turned on
  Draw(*sdl_draw_surface, *sdl_texture, *sdl_renderer, *primary_renderer);
  if (settings->spectator_window) {
    // gpu_world_src is a strict one-shot: it is non-null only for the single
    // Flip that immediately follows the SpectatorViewport::Draw which set it.
    // Any other frame — a menu/pause/weapon-select frame, or a direct Flip()
    // such as MainMenuState::Enter's — finds it null and presents bmp via the
    // CPU path, so a stale handoff can never blit a freed/mismatched buffer.
    if (single_screen_renderer.gpu_world_src) {
      DrawSpectatorGpu(single_screen_renderer);
    } else {
      Draw(*sdl_spectator_draw_surface, *sdl_spectator_texture, *sdl_spectator_renderer,
           single_screen_renderer);
      // This CPU present fully overwrote the overlay texture; force the next GPU
      // frame to re-upload the whole overlay rather than just its dirty bands.
      spectator_prev_present_gpu = false;
    }
    single_screen_renderer.gpu_world_src = nullptr;
  }

  static unsigned int const kDelay = 14U;

  auto wanted_time = last_frame + kDelay;

  while (true) {
    auto now = SDL_GetTicks();
    if (now >= wanted_time) {
      break;
    }

    SDL_Delay(static_cast<uint32_t>(wanted_time - now));
  }

  last_frame = wanted_time;
}

static void PlayChangeSound(Common& common, int change) {
  if (change > 0) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
  } else {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
  }
}

static void ResetLeftRight() {
  gfx.ReleaseSdlKey(SDL_SCANCODE_LEFT);
  gfx.ReleaseSdlKey(SDL_SCANCODE_RIGHT);
}

template <typename T>
static void ChangeVariable(T& var, T change, T min, T max, T scale) {
  if (change < 0 && var > min) {
    var += change * scale;
  }
  if (change > 0 && var < max) {
    var += change * scale;
  }
}

struct ProfileLoadBehavior : ItemBehavior {
  ProfileLoadBehavior(Common& common, WormSettings& ws) : common(common), ws(ws) {}

  Common& common;
  WormSettings& ws;
};

struct LevelSelectBehavior : ItemBehavior {
  LevelSelectBehavior(Common& common) : common(common) {}

  void OnUpdate(Menu& menu, MenuItem& item) override {
    item.has_value = true;
    if (!gfx.settings->random_level) {
      item.value = '"' + GetBasename(GetLeaf(gfx.settings->level_file)) + '"';
      menu.ItemFromId(SettingsMenu::kSiRegenerateLevel)->string = LS(ReloadLevel);  // Not string?
    } else {
      item.value = LS(Random2);
      menu.ItemFromId(SettingsMenu::kSiRegenerateLevel)->string = LS(RegenLevel);
    }
  }

  Common& common;
};

struct WeaponOptionsBehavior : ItemBehavior {
  WeaponOptionsBehavior(Common& common) : common(common) {}

  Common& common;
};

struct OptionsSaveBehavior : ItemBehavior {
  OptionsSaveBehavior(Common& common) : common(common) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    item.value = GetBasename(GetLeaf(gfx.settings_node.FullPath()));
    item.has_value = true;
  }

  Common& common;
};

struct OptionsSelectBehavior : ItemBehavior {
  OptionsSelectBehavior(Common& common) : common(common) {}

  Common& common;
};

ItemBehavior* SettingsMenu::GetItemBehavior(Common& common, MenuItem& item) {
  switch (item.id) {
    case kSiNamesOnBonuses:
      return new BooleanSwitchBehavior(common, gfx.settings->names_on_bonuses);
    case kSiMap:
      return new BooleanSwitchBehavior(common, gfx.settings->map);
    case kSiRegenerateLevel:
      return new BooleanSwitchBehavior(common, gfx.settings->regenerate_level);
    case kSiLoadingTimes:
      return new IntegerBehavior(common, gfx.settings->loading_time, 0, 9999, 1,
                                 /*percentage=*/true);
    case kSiMaxBonuses:
      return new IntegerBehavior(common, gfx.settings->max_bonuses, 0, 99, 1);
    case kSiAmountOfBlood: {
      auto* ret = new IntegerBehavior(common, gfx.settings->blood, 0, LC(BloodLimit),
                                      LC(BloodStepUp), /*percentage=*/true);
      ret->allow_entry = false;
      return ret;
    }

    case kSiLives:
      return new IntegerBehavior(common, gfx.settings->lives, 1, 999, 1);
    case kSiTimeToLose:
    case kSiTimeToWin:
      return new TimeBehavior(common, gfx.settings->time_to_lose, 60, 3600, 10);
    case kSiZoneTimeout:
      return new TimeBehavior(common, gfx.settings->zone_timeout, 10, 3600, 10);
    case kSiFlagsToWin:
      return new IntegerBehavior(common, gfx.settings->flags_to_win, 1, 999, 1);

    case kSiLevel:
      return new LevelSelectBehavior(common);
    case kSiRandomMapWidth:
      return new IntegerBehavior(common, gfx.settings->random_map_width, 64, 4096, 8);
    case kSiRandomMapHeight:
      return new IntegerBehavior(common, gfx.settings->random_map_height, 64, 4096, 8);

    case kSiGameMode:
      return new ArrayEnumBehavior(common, gfx.settings->game_mode, common.texts.game_modes);
    case kSiWeaponOptions:
      return new WeaponOptionsBehavior(common);
    case kLoadOptions:
      return new OptionsSelectBehavior(common);
    case kSaveOptions:
      return new OptionsSaveBehavior(common);
    case kLoadChange:
      return new BooleanSwitchBehavior(common, gfx.settings->load_change);
    default:
      return Menu::GetItemBehavior(common, item);
  }
}

void SettingsMenu::OnUpdate() {
  SetVisibility(kSiLives, /*state=*/false);
  SetVisibility(kSiTimeToLose, /*state=*/false);
  SetVisibility(kSiTimeToWin, /*state=*/false);
  SetVisibility(kSiZoneTimeout, /*state=*/false);
  SetVisibility(kSiFlagsToWin, /*state=*/false);
  SetVisibility(kSiRandomMapWidth, gfx.settings->random_level);
  SetVisibility(kSiRandomMapHeight, gfx.settings->random_level);

  switch (gfx.settings->game_mode) {
    case Settings::kGmKillEmAll:
    case Settings::kGmScalesOfJustice:
      SetVisibility(kSiLives, /*state=*/true);
      break;

    case Settings::kGmGameOfTag:
      SetVisibility(kSiTimeToLose, /*state=*/true);
      break;

    case Settings::kGmHoldazone:
      SetVisibility(kSiTimeToWin, /*state=*/true);
      SetVisibility(kSiZoneTimeout, /*state=*/true);
      break;

    default:
      break;
  }
}

void PlayerMenu::DrawItemOverlay(Common& /*common*/, MenuItem& item, int x, int y, bool selected,
                                 bool /*disabled*/) {
  if (item.id >= PlayerMenu::kPlRed && item.id <= PlayerMenu::kPlBlue)  // Color settings
  {
    int const kRgbcol = item.id - PlayerMenu::kPlRed;
    // Bar geometry predates 8-bit channels: map 0..255 back to 0..63 pixels.
    int const kBarWidth = ws->rgb[kRgbcol] >> 2;

    if (selected) {
      DrawRoundedBox(gfx.play_renderer.bmp, x + 24, y, 168, 7, kBarWidth - 1);
    } else  // CE98
    {
      DrawRoundedBox(gfx.play_renderer.bmp, x + 24, y, 0, 7, kBarWidth - 1);
    }

    FillRect(gfx.play_renderer.bmp, x + 25, y + 1, kBarWidth, 5, ws->color);
  }  // CED9
}

ItemBehavior* PlayerMenu::GetItemBehavior(Common& common, MenuItem& item) {
  if (item.id >= kPlWeap0 && item.id < kPlWeap0 + 5) {
    return new WeaponEnumBehavior(common, ws->weapons[item.id - kPlWeap0]);
  }

  switch (item.id) {
    case kPlName:
      return new WormNameBehavior(common, *ws);
    case kPlHealth: {
      auto* b = new IntegerBehavior(common, ws->health, 1, 10000, 1, /*percentage=*/true);
      b->scroll_interval = 4;
      return b;
    }

    case kPlRed:
    case kPlGreen:
    case kPlBlue: {
      bool const kClassic = gfx.play_renderer.mode == ColorMode::kClassic;
      // Classic mode reproduces the original VGA picker exactly: 64
      // positions shown as 0..63 (stored on the 0..252 grid). Modern mode
      // moves one value at a time across the full 0..255 range, with a
      // faster repeat so a sweep takes about as long as in classic.
      auto* b = new IntegerBehavior(common, ws->rgb[item.id - kPlRed], 0, kClassic ? 252 : 255,
                                    kClassic ? 4 : 1, /*percentage=*/false);
      b->display_div = kClassic ? 4 : 1;
      b->scroll_interval = kClassic ? 4 : 1;
      return b;
    }

    case kPlInput:
      return new InputDeviceBehavior(common, *ws);

    case kPlUp:  // D2AB
    case kPlDown:
    case kPlLeft:
    case kPlRight:
    case kPlFire:
    case kPlChange:
    case kPlJump:
      return new KeyBehavior(
          common, ws->controls[item.id - kPlUp], ws->controls_ex[item.id - kPlUp],
          ws->gamepad_controls[item.id - kPlUp], ws->input_device, Settings::kExtensions);

    case kPlDig:  // Controls Extension
      return new KeyBehavior(
          common, ws->controls_ex[item.id - kPlUp], ws->controls_ex[item.id - kPlUp],
          ws->gamepad_controls[item.id - kPlUp], ws->input_device, Settings::kExtensions);

    case kPlController:  // Controller
      return new ArrayEnumBehavior(common, ws->controller, common.texts.controllers);

    case kPlSaveProfile:  // Save profile
      return new ProfileSaveBehavior(common, *ws, /*save_as=*/false);

    case kPlSaveProfileAs:  // Save profile as
      return new ProfileSaveBehavior(common, *ws, /*save_as=*/true);

    case kPlLoadProfile:
      return new ProfileLoadBehavior(common, *ws);

    case kPlLoadedProfile:
      return new ProfileLoadedBehavior(common, *ws);

    default:
      return Menu::GetItemBehavior(common, item);
  }
}

void Gfx::PlayerSettings(int player) {
  player_menu.ws = settings->worm_settings[player];

  player_menu.UpdateItems(*common);
  player_menu.MoveToFirstVisible();

  cur_menu = &player_menu;
}

void Gfx::InitFrameStepping() {
  tcChangeRequested_ = false;

  controller = std::make_unique<LocalController>(common, settings);

  {
    Level new_level(*common);
    new_level.GenerateFromSettings(*common, *settings, rand);
    controller->SwapLevel(new_level);
  }

  controller->CurrentGame()->Focus(this->play_renderer);
  controller->CurrentGame()->Focus(this->single_screen_renderer);

  // Draw the initial game state so the menu has a proper background
  play_renderer.Clear();
  controller->Draw(this->play_renderer, /*use_spectator_viewports=*/false);
  single_screen_renderer.Clear();
  single_screen_renderer.gpu_world_composite = SpectatorGpuComposite();
  single_screen_renderer.gpu_world_src = nullptr;
  controller->Draw(this->single_screen_renderer, /*use_spectator_viewports=*/true);

  // Push the initial menu state
  auto menu_state = std::make_unique<MainMenuState>();
  menuStatePtr_ = menu_state.get();
  state_stack.Push(std::move(menu_state), this);
}

bool Gfx::RunOneFrame() {
  ZoneScopedN("RunOneFrame");
  if (state_stack.Empty()) {
    return false;
  }

  // Poll events
  SDL_Event ev;
  key_buf_ptr = key_buf;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_EVENT_QUIT) {
      return false;
    }
    if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_F4 &&
        (ev.key.mod & SDL_KMOD_ALT)) {
      return false;
    }
    state_stack.HandleEvent(ev);
  }

  // Capture menu selection before update() might pop and destroy the state
  int const kMenuSelection = menuStatePtr_ ? menuStatePtr_->Selection() : -1;
  bool const kMenuFadingOut = menuStatePtr_ && menuStatePtr_->IsFadingOut();

  if (!state_stack.Update()) {
    // Top state popped. Determine what to do next.
    if (kMenuSelection >= 0) {
      menuStatePtr_ = nullptr;

      if (kMenuSelection == MainMenu::kMaQuit) {
        return false;
      }

      if (kMenuSelection == MainMenu::kMaTc) {
        tcChangeRequested_ = true;
        controller.reset();
        return false;
      }

      // Handle new game / resume / replay selection
      if (kMenuSelection == MainMenu::kMaNewGame) {
        net_session.reset();
        std::unique_ptr<Controller> new_controller(new LocalController(common, settings));
        Level* old_level = controller->CurrentLevel();

        if (old_level && !settings->regenerate_level &&
            settings->random_level == old_level->old_random_level &&
            settings->level_file == old_level->old_level_file &&
            settings->random_map_width == old_level->old_random_map_width &&
            settings->random_map_height == old_level->old_random_map_height) {
          new_controller->SwapLevel(*old_level);
        } else {
          Level new_level(*common);
          new_level.GenerateFromSettings(*common, *settings, rand);
          new_controller->SwapLevel(new_level);
        }
        controller = std::move(new_controller);
      } else if (kMenuSelection == MainMenu::kMaResumeGame) {
        if (controller->IsReplay()) {
          // single_screen_renderer is drawn to the main window in replay mode,
          // so it must fit in the main window surface (max 640×400).
          single_screen_renderer.SetRenderResolution(640, 400);
          primary_renderer = &single_screen_renderer;
        }
      } else if (kMenuSelection == MainMenu::kMaReplay) {
        if (settings->single_screen_replay) {
          single_screen_renderer.SetRenderResolution(640, 400);
          primary_renderer = &single_screen_renderer;
        }
      } else if (kMenuSelection == MainMenu::kMaHostGame) {
        state_stack.Push(std::make_unique<NetConnectState>(NetSession::kHost, "", gfx.online_port),
                         this);
        return true;
      } else if (kMenuSelection == MainMenu::kMaJoinGame) {
        // Parse address — support "host:port" and "[ipv6]:port" formats
        std::string addr = std::move(pending_net_address);
        uint16_t port = gfx.online_port;

        if (!addr.empty() && addr[0] == '[') {
          // IPv6 bracket notation: [::1]:port
          auto close_bracket = addr.find(']');
          if (close_bracket != std::string::npos) {
            std::string const kIp6 = addr.substr(1, close_bracket - 1);
            if (close_bracket + 1 < addr.size() && addr[close_bracket + 1] == ':') {
              try {
                port = static_cast<uint16_t>(std::stoi(addr.substr(close_bracket + 2)));
              } catch (...) {  // NOLINT(bugprone-empty-catch) — malformed port string falls back to
                               // the default.
                // Malformed port, keep default
              }
            }
            addr = kIp6;
          }
        } else {
          // IPv4 or hostname: check for last colon
          auto last_colon = addr.rfind(':');
          if (last_colon != std::string::npos) {
            // Only treat as port separator if there's at most one colon
            // (multiple colons = bare IPv6 without port)
            auto first_colon = addr.find(':');
            if (first_colon == last_colon) {
              try {
                port = static_cast<uint16_t>(std::stoi(addr.substr(last_colon + 1)));
              } catch (...) {  // NOLINT(bugprone-empty-catch) — malformed port string falls back to
                               // the default.
                // Malformed port, keep default
              }
              addr = addr.substr(0, last_colon);
            }
          }
        }

        state_stack.Push(
            std::make_unique<NetConnectState>(NetSession::kClient, std::move(addr), port), this);
        return true;
      } else if (kMenuSelection == MainMenu::kMaHostOnline) {
        state_stack.Push(std::make_unique<OnlineConnectState>(NetSession::kHost), this);
        return true;
      } else if (kMenuSelection == MainMenu::kMaJoinOnline) {
        std::string code = std::move(pending_net_address);
        state_stack.Push(std::make_unique<OnlineConnectState>(NetSession::kClient, std::move(code)),
                         this);
        return true;
      }

      // Push game state
      state_stack.Push(std::make_unique<GamePlayState>(), this);
    } else {
      // Game state finished — go back to menu
      net_session.reset();
      primary_renderer = &play_renderer;
      controller->Unfocus();
      ClearKeys();

      // Restore the spectator renderer to its windowed render resolution now
      // that it's no longer shared with the main window. Route through
      // OnWindowResize so the render resolution, the SDL logical presentation,
      // and the textures are all re-derived together (including the render
      // cap). Setting render_res to the raw window size here instead would
      // desync it from the capped logical presentation: the world dst rect
      // would be computed in the larger window space while SDL clips it to
      // the smaller capped canvas, pushing the right/bottom of the world —
      // and any worm there — off-screen.
      if (sdl_spectator_window && settings->spectator_window) {
        OnWindowResize(SDL_GetWindowID(sdl_spectator_window));
      }

      // Draw one frame so the menu background captures the final game state
      play_renderer.Clear();
      controller->Draw(this->play_renderer, /*use_spectator_viewports=*/false);
      single_screen_renderer.Clear();
      single_screen_renderer.gpu_world_composite = SpectatorGpuComposite();
      single_screen_renderer.gpu_world_src = nullptr;
      controller->Draw(this->single_screen_renderer, /*use_spectator_viewports=*/true);

      auto new_menu = std::make_unique<MainMenuState>();
      menuStatePtr_ = new_menu.get();
      state_stack.Push(std::move(new_menu), this);
    }
    return true;
  }

  // Menu states finalize their palettes before drawing (blits resolve
  // through pal32 at draw time); game states rebuild inside Game::Draw.
  auto* top = state_stack.Top();
  bool const kUseMenuFlip = !top || top->WantsMenuFlip();
  if (kUseMenuFlip) {
    UpdateMenuPalettes(kMenuFadingOut);
  }

  // Decide per frame whether the spectator viewport hands its world to the GPU;
  // reset the handoff so a frame that doesn't redraw the viewport (e.g. a menu
  // over a frozen game) falls back to the CPU present path.
  single_screen_renderer.gpu_world_composite = SpectatorGpuComposite();
  single_screen_renderer.gpu_world_src = nullptr;

  state_stack.Draw();

  if (!kUseMenuFlip) {
    ++menu_cycles;
  }
  Flip();
  FrameMark;

  return true;
}

void Gfx::MainLoop() {
  InitFrameStepping();

#if OPENLIERO_EMSCRIPTEN
  emscripten_set_main_loop_arg(
      [](void* arg) {
        Gfx* self = static_cast<Gfx*>(arg);
        if (!self->RunOneFrame()) {
          if (self->TcChangeRequested()) {
            self->InitFrameStepping();
          } else {
            self->controller.reset();
            emscripten_cancel_main_loop();
          }
        }
      },
      this, 0, true);
#else
  while (true) {
    while (RunOneFrame()) {
    }

    if (!TcChangeRequested()) {
      break;
    }

    // TC was changed (common reloaded by TcSelectorState) — reinitialize
    InitFrameStepping();
  }

  controller.reset();
#endif
}

void Gfx::SaveSettings(const FsNode& node) {
  settings_node = node;
  settings->save(node, rand);
}

bool Gfx::LoadSettings(const FsNode& node) {
  settings_node = node;
  settings = std::make_shared<Settings>();
  return settings->load(node, rand);
}

void Gfx::DrawBasicMenu(/*int curSel*/) {
  play_renderer.bmp.Copy(frozen_screen);

  main_menu.Draw(*common, play_renderer, cur_menu != &main_menu, -1,
                 /*show_disabled_selection=*/true);
}

void Gfx::SetSpectatorLayout(bool fixed) {
  if (primary_renderer == &single_screen_renderer) {
    return;  // single-screen replay: renderer is the primary; don't resize it
  }
  if (!sdl_spectator_window || !settings->spectator_window) {
    return;
  }
  int target_w = 0;
  int target_h = 0;
  if (fixed) {
    target_w = 640;
    target_h = 400;
  } else {
    int win_w = 0;
    int win_h = 0;
    SDL_GetWindowSize(sdl_spectator_window, &win_w, &win_h);
    // Match per-frame render resolution to the capped textures and SDL logical
    // presentation set by OnWindowResize. Without this, on a window taller than
    // the cap (e.g. 3440x1440 → 2580x1080) render_res would be reset to the
    // full window every frame while the HUD and world textures stay capped:
    // the HUD's bottom rows then fall outside the shorter texture (HUD vanishes)
    // and the world is clipped to the capped canvas (worms can leave the screen).
    CappedRenderResolution const kCap =
        ComputeCappedRenderResolution(win_w, win_h, settings->max_spectator_render_height);
    target_w = kCap.w;
    target_h = kCap.h;
  }
  if (single_screen_renderer.render_res_x != target_w ||
      single_screen_renderer.render_res_y != target_h) {
    single_screen_renderer.SetRenderResolution(target_w, target_h);
  }
}

void Gfx::DrawSpectatorInfo() {
  Common& common = *this->common;
  int const kCenterX = single_screen_renderer.render_res_x / 2;
  int const kCenterY = single_screen_renderer.render_res_y / 4;

  Fill(single_screen_renderer.bmp, 0);
  if (frozen_spectator_screen.pixels != nullptr) {
    BlitBitmap(single_screen_renderer.bmp, frozen_spectator_screen, 0, 0, frozen_spectator_screen.w,
               frozen_spectator_screen.h);
  }
  if (settings->level_file.empty()) {
    common.font.DrawCenteredText(single_screen_renderer.bmp, LS(LevelRandom), kCenterX,
                                 kCenterY - 32, 7, 2);
  } else {
    auto level_name = GetBasename(GetLeaf(gfx.settings->level_file));
    common.font.DrawCenteredText(single_screen_renderer.bmp,
                                 LS(LevelIs1) + level_name + LS(LevelIs2), kCenterX, kCenterY - 32,
                                 7, 2);
  }

  std::string const kVsText =
      settings->worm_settings[0]->name + " vs " + settings->worm_settings[1]->name;
  // put worm color boxes on a nice spot even if no player names have been entered
  int const kTextSize = std::max(common.font.GetDims(kVsText) * 2, 48);
  common.font.DrawCenteredText(single_screen_renderer.bmp, kVsText, kCenterX, kCenterY, 7, 2);
  FillRect(single_screen_renderer.bmp, kCenterX - (kTextSize / 2) - 1, kCenterY + 23 - 1, 16, 16,
           7);
  FillRect(single_screen_renderer.bmp, kCenterX - kTextSize / 2, kCenterY + 23, 14, 14,
           settings->worm_settings[0]->color);
  FillRect(single_screen_renderer.bmp, kCenterX + (kTextSize / 2) - 16 - 1, kCenterY + 23 - 1, 16,
           16, 7);
  FillRect(single_screen_renderer.bmp, kCenterX + kTextSize / 2 - 16, kCenterY + 23, 14, 14,
           settings->worm_settings[1]->color);

  if (controller->Running()) {
    common.font.DrawCenteredText(single_screen_renderer.bmp, "PAUSED", kCenterX, kCenterY + 48, 7,
                                 2);
  } else {
    common.font.DrawCenteredText(single_screen_renderer.bmp, "SETUP", kCenterX, kCenterY + 48, 7,
                                 2);
  }
}

static int UpperCaseOnly(int k) {
  k = std::toupper(k);

  if ((k >= 'A' && k <= 'Z') || (k == 0x8f || k == 0x8e || k == 0x99)  // � �and �
      || (k >= '0' && k <= '9')) {
    return k;
  }

  return 0;
}

void Gfx::OpenHiddenMenu() {
  if (cur_menu == &hidden_menu) {
    return;
  }
  cur_menu = &hidden_menu;
  cur_menu->UpdateItems(*common);
  cur_menu->MoveToFirstVisible();
}
