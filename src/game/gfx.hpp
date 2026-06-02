#pragma once

#include <SDL3/SDL.h>
#include "math/rect.hpp"

#include <cassert>
#include <cstdio>
#include <unordered_map>

#include "common.hpp"
#include "filesystem.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/blit.hpp"
#include "gfx/color.hpp"
#include "gfx/font.hpp"
#include "gfx/renderer.hpp"
#include "keys.hpp"
#include "menu/hiddenMenu.hpp"
#include "menu/mainMenu.hpp"
#include "menu/menu.hpp"
#include "mixer/player.hpp"
#include "rand.hpp"
#include "settings.hpp"
#include "state.hpp"

struct Key {
  Key(int sym, char ch) : sym(sym), ch(ch) {}

  int sym;
  char ch;
};

struct Game;
struct Controller;
struct Gfx;
struct NetSession;

struct PlayerMenu : Menu {
  PlayerMenu(int x, int y) : Menu(x, y) {}

  enum {
    kPlName,
    kPlHealth,
    kPlRed,
    kPlGreen,
    kPlBlue,
    kPlInput,
    kPlUp,
    kPlDown,
    kPlLeft,
    kPlRight,
    kPlFire,
    kPlChange,
    kPlJump,
    kPlDig,
    kPlWeap0,
    kPlController = kPlWeap0 + 5,
    kPlSaveProfile,
    kPlSaveProfileAs,
    kPlLoadProfile,
    kPlLoadedProfile,
  };

  virtual void DrawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected,
                               bool disabled);

  virtual ItemBehavior* GetItemBehavior(Common& common, MenuItem& item);

  std::shared_ptr<WormSettings> ws;
};

struct SettingsMenu : Menu {
  enum {
    kSiGameMode,
    kSiLives,
    kSiTimeToLose,  // Extra
    kSiTimeToWin,
    kSiZoneTimeout,
    kSiFlagsToWin,  // Extra
    kSiLoadingTimes,
    kSiMaxBonuses,
    kSiNamesOnBonuses,
    kSiMap,
    kSiAmountOfBlood,
    kSiLevel,
    kSiRegenerateLevel,
    kSiWeaponOptions,
    kLoadOptions,
    kSaveOptions,
    kLoadChange,
  };

  SettingsMenu(int x, int y) : Menu(x, y) {}

  virtual ItemBehavior* GetItemBehavior(Common& common, MenuItem& item);

  virtual void OnUpdate();
};

struct Joystick {
  SDL_Gamepad* sdl_gamepad;
  SDL_JoystickID instance_id;
  bool btn_state[SDL_GAMEPAD_BUTTON_COUNT];
  bool btn_pressed[SDL_GAMEPAD_BUTTON_COUNT];  // Latched on press, cleared by testGamepadButtonOnce
  bool axis_button_state[12];                  // 6 axes * 2 directions
  bool axis_pressed[12];  // Latched on axis threshold cross, cleared by consumer

  void ClearState() {
    std::memset(btn_state, 0, sizeof(btn_state));
    std::memset(btn_pressed, 0, sizeof(btn_pressed));
    std::memset(axis_button_state, 0, sizeof(axis_button_state));
    std::memset(axis_pressed, 0, sizeof(axis_pressed));
  }
};

struct Gfx {
  Gfx();

  void Init();
  void SetVideoMode();
  void OnWindowResize(uint32_t window_id);
  void LoadMenus();

  void Process(Controller* controller = 0);
  // draws a given surface onto an SDL texture/renderer, using a given Renderer
  void Draw(SDL_Surface& surface, SDL_Texture& texture, SDL_Renderer& sdl_renderer,
            Renderer& renderer);
  void Flip();
  void MenuFlip(bool quitting = false);

  void ClearKeys();

  bool TestKeyOnce(uint32_t key) {
    bool state = dos_keys[key];
    dos_keys[key] = false;
    return state;
  }

  bool TestKey(uint32_t key) { return dos_keys[key]; }

  void ReleaseKey(uint32_t key) { dos_keys[key] = false; }

  void PressKey(uint32_t key) { dos_keys[key] = true; }

  void SetKey(uint32_t key, bool state) { dos_keys[key] = state; }

  void ToggleKey(uint32_t key) { dos_keys[key] = !dos_keys[key]; }

  bool TestSdlKeyOnce(SDL_Scancode key) {
    uint32_t k = SDLToDOSKey(key);
    return k ? TestKeyOnce(k) : false;
  }

  bool TestSdlKey(SDL_Scancode key) {
    uint32_t k = SDLToDOSKey(key);
    return k ? TestKey(k) : false;
  }

  void ReleaseSdlKey(SDL_Scancode key) {
    uint32_t k = SDLToDOSKey(key);
    if (k) dos_keys[k] = false;
  }

  // Test any key (both regular DOS keys and extended joystick keys)
  bool TestAnyKeyOnce(uint32_t key) {
    if (key == 0) return false;
    if (key < kMaxDosKey) return TestKeyOnce(key);
    auto it = ex_keys.find(key);
    if (it != ex_keys.end() && it->second) {
      it->second = false;
      return true;
    }
    return false;
  }

  bool TestAnyKey(uint32_t key) {
    if (key == 0) return false;
    if (key < kMaxDosKey) return TestKey(key);
    auto it = ex_keys.find(key);
    return it != ex_keys.end() && it->second;
  }

  void ReleaseAnyKey(uint32_t key) {
    if (key == 0) return;
    if (key < kMaxDosKey)
      dos_keys[key] = false;
    else
      ex_keys[key] = false;
  }

  // Test if any player's configured control for a given action was pressed.
  // Uses controlsEx which covers both keyboard and joystick bindings.
  bool TestControlOnce(int control);

  // Test if any connected gamepad has a raw button pressed (one-shot)
  bool TestGamepadButtonOnce(int button);

  // Test if any connected gamepad has a raw button held (non-destructive)
  bool TestGamepadButton(int button);

  // Directional input: checks both DPad button AND left stick axis (one-shot)
  bool TestGamepadDirOnce(int dpad_button);

  // Directional input: checks both DPad button AND left stick axis (held)
  bool TestGamepadDir(int dpad_button);

  // Non-destructive version for held keys (left/right repeat)
  bool TestControl(int control);

  // Release a control key for all players
  void ReleaseControl(int control);

  std::string GetKeyName(uint32_t key);
  std::string GetGamepadKeyName(uint32_t gamepad_key);
  void SetSpectatorFullscreen(bool new_fullscreen);
  void SetFullscreen(bool new_fullscreen);
  void SetDoubleRes(bool new_double_res);

  void SaveSettings(FsNode node);
  bool LoadSettings(FsNode node);

  void ProcessEvent(SDL_Event& ev, Controller* controller = 0);

  int FindGamepadIndex(SDL_JoystickID id);
  int FindGamepadForPlayer(int player_idx);
  void DispatchGamepadInput(int gp_idx, uint32_t gamepad_key, bool state, Controller* controller);

  void MainLoop();

  // Initialize the state stack.
  // Call once before calling runOneFrame() in a loop.
  void InitFrameStepping();

  // Advance the application by one frame. Returns false when the
  // application should exit (quit selected or TC change requested).
  // After a TC change, call initFrameStepping() again.
  bool RunOneFrame();

  // True if a TC change was requested (caller should reload and re-init)
  bool TcChangeRequested() const { return tcChangeRequested_; }

  void DrawBasicMenu(/*int curSel*/);
  void DrawSpectatorInfo();
  void PlayerSettings(int player);
  void OpenHiddenMenu();

  static void PreparePalette(SDL_PixelFormatDetails const* format, SDL_Palette const* palette,
                             Color real_pal[256], uint32_t (&pal32)[256]);

  static void Overlay(SDL_PixelFormatDetails const* format, uint8_t* src, int w, int h,
                      std::size_t src_pitch, uint8_t* dest, std::size_t dest_pitch, int mag);

  void SetConfigNodes(FsNode const& config, FsNode const& user_config) {
    config_node = config;
    user_config_node = user_config;
  }

  FsNode GetConfigNode() { return config_node; }

  FsNode GetUserConfigNode() { return user_config_node; }

  // PRNG for things that don't affect the game
  Rand rand;

  // renders everything for actual play
  Renderer play_renderer;
  // renders everything on a single screen, for single screen replay and
  // the spectator window
  Renderer single_screen_renderer;

  // the renderer currently in use by the primary window. Usually
  // playRenderer, but is singleScreenRenderer if watching a replay in
  // single screen mode.
  Renderer* primary_renderer;

  FsNode config_node;
  FsNode user_config_node;

  // Port for online play (default 19532, configurable via --port)
  uint16_t online_port = 19532;

  MainMenu main_menu;
  SettingsMenu settings_menu;
  PlayerMenu player_menu;
  HiddenMenu hidden_menu;

  Menu* cur_menu;
  std::string prev_selected_replay_path;
  FsNode settings_node;  // Currently loaded settings file. TODO: This is only used for display. We
                         // could just remember the name.
  std::shared_ptr<Settings> settings;

  bool dos_keys[177];
  std::unordered_map<uint32_t, bool> ex_keys;

  // the window to render into
  SDL_Window* sdl_window = NULL;
  // the window to render the spectator view into
  SDL_Window* sdl_spectator_window = NULL;
  // the SDL renderer to use
  SDL_Renderer* sdl_renderer = NULL;
  // the SDL renderer to use for the spectator window
  SDL_Renderer* sdl_spectator_renderer = NULL;
  // full window size texture that represents the window
  SDL_Texture* sdl_texture = NULL;
  // full spectator window size texture that represents the spectator window
  SDL_Texture* sdl_spectator_texture = NULL;
  // a software surface to do the actual drawing into
  SDL_Surface* sdl_draw_surface = NULL;
  // a software surface to do the actual drawing of the spectator view into
  SDL_Surface* sdl_spectator_draw_surface = NULL;
  // when the menu is open, the ongoing game on the screen is paused and
  // stored in this bitmap
  Bitmap frozen_screen;
  // when the menu is open, the ongoing game on the spectator screen is
  // paused and stored in this bitmap
  Bitmap frozen_spectator_screen;

  bool running;
  bool spectator_fullscreen, double_res;

  uint64_t last_frame;
  unsigned menu_cycles;
  int window_w, window_h;
  int prev_mag;           // Previous magnification used for drawing
  Rect last_update_rect;  // Last region that was updated when flipping
  std::shared_ptr<Common> common;
  std::shared_ptr<SoundPlayer> sound_player;
  std::unique_ptr<Controller> controller;
  std::unique_ptr<NetSession> net_session;
  std::string pending_net_address;

  StateStack state_stack;

  // Used by sub-states to communicate a menu selection back to MainMenuState
  int pending_menu_selection = -1;

  // Error message to display after GamePlayState pops (set by controllers)
  std::string pending_error_message;

  std::vector<Joystick> joysticks;

  SDL_Scancode key_buf[32], *key_buf_ptr;

  std::vector<std::pair<int, int>> debug_points;
  std::string debug_info;

 private:
  struct MainMenuState* menuStatePtr_ = nullptr;
  bool tcChangeRequested_ = false;
};

extern Gfx gfx;
