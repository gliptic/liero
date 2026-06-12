#pragma once

#include <xxhash.h>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include "filesystem.hpp"
#include "math.hpp"
#include "rand.hpp"

#define NUM_WEAPONS 5

struct Worm;
struct Game;
struct Weapon;

struct Ninjarope {
  Ninjarope() = default;

  bool out{false};  // Is the ninjarope out?
  bool attached{false};
  Worm* anchor{nullptr};  // If non-zero, the worm the ninjarope is attached to
  fixedvec pos, vel;      // Ninjarope props
  // Not needed as far as I can tell: fixed forceX, forceY;
  int length, cur_len;

  void Process(Worm& owner, Game& game);
};

struct WormWeapon {
  WormWeapon() = default;

  bool Available() const { return loading_left == 0; }

  // int id;
  Weapon const* type;
  int ammo{0};
  int delay_left{0};
  int loading_left{0};
};

struct WormSettingsExtensions {
  enum Control {
    kUp = 0,
    kDown = 1,
    kLeft = 2,
    kRight = 3,
    kFire = 4,
    kChange = 5,
    kJump = 6,
    kDig = 7,
    kMaxControl = kDig,
    kMaxControlEx = 8
  };

  // Input device: 0 = keyboard, 1 = gamepad 0, 2 = gamepad 1, etc.
  static int const kInputKeyboard = 0;

  WormSettingsExtensions() {
    std::memset(controls_ex, 0, sizeof(controls_ex));
    std::memset(gamepad_controls, 0, sizeof(gamepad_controls));

    InitDefaultGamepadControls();
  }

  void InitDefaultGamepadControls();

  uint32_t controls_ex[kMaxControlEx];

  // Gamepad binding: stores SDL_GamepadButton or encoded axis for each control
  // Encoding: 0-99 = SDL_GamepadButton, 100+axis*2 = axis positive, 101+axis*2 = axis negative
  static uint32_t const kGamepadAxisBase = 100;
  static uint32_t GamepadAxisPositive(int axis) { return kGamepadAxisBase + axis * 2; }
  static uint32_t GamepadAxisNegative(int axis) { return kGamepadAxisBase + axis * 2 + 1; }
  static bool IsGamepadAxis(uint32_t v) { return v >= kGamepadAxisBase; }

  uint32_t gamepad_controls[kMaxControlEx];
  uint32_t input_device{kInputKeyboard};
  std::string gamepad_name;    // Human-readable name (e.g., "Xbox Wireless Controller")
  std::string gamepad_serial;  // Hardware serial for disambiguating identical controllers
};

struct WormSettings : WormSettingsExtensions {
  WormSettings() {
    rgb[0] = 104;
    rgb[1] = 104;
    rgb[2] = 248;

    for (unsigned int& weapon : weapons) {
      weapon = 1;
    }
    std::memset(controls, 0, sizeof(controls));
  }

  uint64_t& UpdateHash();

  void SaveProfile(const FsNode& node);
  void LoadProfile(const FsNode& node);
  std::string ToToml() const;
  void FromToml(std::string const& data);

  int health{100};
  uint32_t controller{0};  // CPU / Human
  uint32_t controls[kMaxControl];
  uint32_t weapons[NUM_WEAPONS];  // TODO: Adjustable
  std::string name;
  int rgb[3];  // 0..255 per channel (classic rendering quantizes to the VGA grid)
  bool random_name{true};

  int color{0};

  // std::string profilePath;
  FsNode profile_node;

  uint64_t hash;
};

struct Viewport;
struct Renderer;

struct WormAI {
  virtual void Process(Game& game, Worm& worm) = 0;

  virtual void DrawDebug(Game& game, Worm const& worm, Renderer& renderer, int offs_x, int offs_y) {
  }
};

struct DumbLieroAI : WormAI {
  void Process(Game& game, Worm& worm) override;

  Rand rand;
};

struct Worm {
  enum { kRfDown, kRfLeft, kRfUp, kRfRight };

  enum Control {
    kUp = WormSettings::kUp,
    kDown = WormSettings::kDown,
    kLeft = WormSettings::kLeft,
    kRight = WormSettings::kRight,
    kFire = WormSettings::kFire,
    kChange = WormSettings::kChange,
    kJump = WormSettings::kJump,
    kMaxControl = 7
  };

  struct ControlState {
    ControlState() = default;

    bool operator==(ControlState const& b) const { return istate == b.istate; }

    uint32_t Pack() const {
      return istate;  // & ((1 << MaxControl)-1);
    }

    void Unpack(uint32_t state) { istate = state & 0x7f; }

    bool operator!=(ControlState const& b) const { return !operator==(b); }

    bool operator[](std::size_t n) const { return ((istate >> n) & 1) != 0; }

    void Set(std::size_t n, bool v) {
      if (v) {
        istate |= 1 << n;
      } else {
        istate &= ~(static_cast<uint32_t>(1U) << n);
      }
    }

    void Toggle(std::size_t n) { istate ^= 1 << n; }

    uint32_t istate{0};
  };

  Worm()
      : ready(true),
        movable(true),
        killed_timer(kKilledTimerInitial)

  {}

  bool Pressed(Control control) const { return control_states[control]; }

  bool PressedOnce(Control control) {
    bool const kState = control_states[control];
    control_states.Set(control, /*v=*/false);
    return kState;
  }

  void Release(Control control) { control_states.Set(control, /*v=*/false); }

  void Press(Control control) { control_states.Set(control, /*v=*/true); }

  void SetControlState(Control control, bool state) { control_states.Set(control, state); }

  void ToggleControlState(Control control) {
    control_states.Set(control, !control_states[control]);
  }

  int MinimapColor() const { return 129 + index * 4; }

  void BeginRespawn(Game& game);
  void DoRespawning(Game& game);
  void Process(Game& game);
  void ProcessWeapons(Game& game);
  void ProcessPhysics(Game& game);
  void ProcessMovement(Game& game);
  void ProcessTasks(Game& game);
  void ProcessAiming(Game& game);
  void ProcessWeaponChange(Game& game);
  void ProcessSteerables(Game& game);
  void Fire(Game& game);
  void ProcessSight(Game& game);
  void CalculateReactionForce(Game& game, int new_x, int new_y, int dir);
  void InitWeapons(Game& game);
  int AngleFrame() const;

  fixedvec pos, vel;

  IVec2 logic_respawn;

  int hotspot_x{0}, hotspot_y{0};  // Hotspots for laser, laser sight, etc.
  fixed aiming_angle{0}, aiming_speed{0};

  bool able_to_jump{false}, able_to_dig{false};  // The previous state of some keys
  bool key_change_pressed{false};
  bool movable{false};

  bool animate{false};           // Should the worm be animated?
  bool visible{false};           // Is the worm visible?
  bool ready{false};             // Is the worm ready to play?
  bool flag{false};              // Does the worm have a flag?
  bool make_sight_green{false};  // Changes the sight color
  int health{0};                 // Health left
  int lives{0};                  // lives left
  int kills{0};                  // Kills made

  int timer{0};         // Timer for GOT
  int killed_timer{0};  // Time until worm respawns
  static constexpr int kKilledTimerInitial = 150;
  int current_frame{0};

  int flags{0};  // How many flags does this worm have?

  Ninjarope ninjarope;

  int current_weapon{0};       // The selected weapon
  int last_killed_by_idx{-1};  // What worm that last killed this worm
  int fire_cone{0};            // How much is left of the firecone
  int leave_shell_timer{0};    // Time until next shell drop

  std::shared_ptr<WormSettings> settings;  // !CLONING
  int index{0};                            // 0 or 1

  std::shared_ptr<WormAI> ai;

  int reacts[4];
  WormWeapon weapons[NUM_WEAPONS];
  int direction{0};
  ControlState control_states;
  ControlState prev_control_states;

  // Temporary state for steerables
  int steerable_sum_x, steerable_sum_y, steerable_count{0};

  // which X coordinate to display stats at for this worm
  int stats_x{0};

  // Data for LocalController
  ControlState clean_control_states;  // This contains the real state of real and
                                      // extended controls
};

bool CheckForWormHit(Game& game, int x, int y, int dist, Worm* own_worm);
bool CheckForSpecWormHit(Game& game, int x, int y, int dist, Worm& w);
int SqrVectorLength(int x, int y);
