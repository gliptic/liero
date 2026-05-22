#pragma once

#include <cstring>
#include <functional>
#include <gvl/crypt/gash.hpp>
#include <gvl/serialization/archive.hpp>
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
  Ninjarope() : out(false), attached(false), anchor(0) {}

  bool out;  // Is the ninjarope out?
  bool attached;
  Worm* anchor;       // If non-zero, the worm the ninjarope is attached to
  fixedvec pos, vel;  // Ninjarope props
  // Not needed as far as I can tell: fixed forceX, forceY;
  int length, curLen;

  void process(Worm& owner, Game& game);
};

struct WormWeapon {
  WormWeapon() : ammo(0), delayLeft(0), loadingLeft(0) {}

  bool available() const { return loadingLeft == 0; }

  // int id;
  Weapon const* type;
  int ammo;
  int delayLeft;
  int loadingLeft;
};

struct WormSettingsExtensions {
  enum Control {
    Up,
    Down,
    Left,
    Right,
    Fire,
    Change,
    Jump,
    Dig,
    MaxControl = Dig,
    MaxControlEx
  };

  // Input device: 0 = keyboard, 1 = gamepad 0, 2 = gamepad 1, etc.
  static int const InputKeyboard = 0;

  WormSettingsExtensions() {
    std::memset(controlsEx, 0, sizeof(controlsEx));
    std::memset(gamepadControls, 0, sizeof(gamepadControls));
    inputDevice = InputKeyboard;
    initDefaultGamepadControls();
  }

  void initDefaultGamepadControls();

  uint32_t controlsEx[MaxControlEx];

  // Gamepad binding: stores SDL_GamepadButton or encoded axis for each control
  // Encoding: 0-99 = SDL_GamepadButton, 100+axis*2 = axis positive, 101+axis*2 = axis negative
  static uint32_t const GamepadAxisBase = 100;
  static uint32_t gamepadAxisPositive(int axis) { return GamepadAxisBase + axis * 2; }
  static uint32_t gamepadAxisNegative(int axis) { return GamepadAxisBase + axis * 2 + 1; }
  static bool isGamepadAxis(uint32_t v) { return v >= GamepadAxisBase; }

  uint32_t gamepadControls[MaxControlEx];
  uint32_t inputDevice;
  std::string gamepadName;   // Human-readable name (e.g., "Xbox Wireless Controller")
  std::string gamepadSerial; // Hardware serial for disambiguating identical controllers
};

struct WormSettings : gvl::shared, WormSettingsExtensions {
  WormSettings() : health(100), controller(0), randomName(true), color(0) {
    rgb[0] = 26;
    rgb[1] = 26;
    rgb[2] = 62;

    for (int i = 0; i < NUM_WEAPONS; ++i) {
      weapons[i] = 1;
    }
    std::memset(controls, 0, sizeof(controls));
  }

  gvl::gash::value_type& updateHash();

  void saveProfile(FsNode node);
  void loadProfile(FsNode node);
  std::string toToml() const;
  void fromToml(std::string const& data);

  int health;
  uint32_t controller;  // CPU / Human
  uint32_t controls[MaxControl];
  uint32_t weapons[NUM_WEAPONS];  // TODO: Adjustable
  std::string name;
  int rgb[3];
  bool randomName;

  int color;

  // std::string profilePath;
  FsNode profileNode;

  gvl::gash::value_type hash;
};

// Shared TOML serialization for worm settings (used by both settings file and profiles)
template <typename Archive>
void archive_worm_toml(Archive& ar, WormSettings& ws) {
  ar.u32("controller", ws.controller);
  if (ar.in)
    ws.controller = ws.controller % 3;
  ar.arr("color", ws.rgb, [&](int& c) {
    ar.i32(0, c);
    if (ar.in)
      c &= 63;
  });
  ar.arr("weapons", ws.weapons, [&](uint32_t& w) { ar.u32(0, w); });
  ar.i32("health", ws.health);

  if (ws.randomName && ar.out) {
    std::string empty;
    ar.str("name", empty);
  } else {
    ar.str("name", ws.name);
    if (ar.in && !ws.name.empty())
      ws.randomName = false;
  }

  ar.arr("controls", ws.controlsEx, [&](uint32_t& c) { ar.u32(0, c); });
  ar.u32("inputDevice", ws.inputDevice);
  ar.str("gamepadName", ws.gamepadName);
  ar.str("gamepadSerial", ws.gamepadSerial);
  ar.arr("gamepadControls", ws.gamepadControls,
         [&](uint32_t& c) { ar.u32(0, c); });
}

// WormSettings archive for replays: embeds TOML as a string in the binary stream.
template <typename Archive>
void archive(Archive ar, WormSettings& ws) {
  if (ar.out) {
    std::string toml = ws.toToml();
    ar.str(toml);
  } else {
    std::string toml;
    ar.str(toml);
    ws.fromToml(toml);
  }
}

struct Viewport;
struct Renderer;

struct WormAI : gvl::shared {
  virtual void process(Game& game, Worm& worm) = 0;

  virtual void drawDebug(
      Game& game,
      Worm const& worm,
      Renderer& renderer,
      int offsX,
      int offsY) {}
};

struct DumbLieroAI : WormAI {
  void process(Game& game, Worm& worm);

  Rand rand;
};

struct Worm : gvl::shared {
  enum { RFDown, RFLeft, RFUp, RFRight };

  enum Control {
    Up = WormSettings::Up,
    Down = WormSettings::Down,
    Left = WormSettings::Left,
    Right = WormSettings::Right,
    Fire = WormSettings::Fire,
    Change = WormSettings::Change,
    Jump = WormSettings::Jump,
    MaxControl
  };

  struct ControlState {
    ControlState() : istate(0) {}

    bool operator==(ControlState const& b) const { return istate == b.istate; }

    uint32_t pack() const {
      return istate;  // & ((1 << MaxControl)-1);
    }

    void unpack(uint32_t state) { istate = state & 0x7f; }

    bool operator!=(ControlState const& b) const { return !operator==(b); }

    bool operator[](std::size_t n) const { return ((istate >> n) & 1) != 0; }

    void set(std::size_t n, bool v) {
      if (v)
        istate |= 1 << n;
      else
        istate &= ~(uint32_t(1u) << n);
    }

    void toggle(std::size_t n) { istate ^= 1 << n; }

    uint32_t istate;
  };

  Worm()
      : pos(),
        vel(),
        hotspotX(0),
        hotspotY(0),
        aimingAngle(0),
        aimingSpeed(0),
        ableToJump(false),
        ableToDig(false)  // The previous state of some keys
        ,
        keyChangePressed(false),
        movable(false),
        animate(false)  // Should the worm be animated?
        ,
        visible(false)  // Is the worm visible?
        ,
        ready(false)  // Is the worm ready to play?
        ,
        flag(false)  // Has the worm a flag?
        ,
        makeSightGreen(false)  // Changes the sight color
        ,
        health(0)  // Health left
        ,
        lives(0)  // lives left
        ,
        kills(0)  // Kills made
        ,
        timer(0)  // Timer for GOT
        ,
        killedTimer(0)  // Time until worm respawns
        ,
        currentFrame(0),
        flags(0)  // How many flags does this worm have?
        ,
        currentWeapon(0),
        lastKilledByIdx(-1),
        fireCone(0),
        leaveShellTimer(0),
        index(0),
        direction(0),
        steerableCount(0),
        statsX(0) {
    makeSightGreen = false;

    ready = true;
    movable = true;

    visible = false;
    killedTimer = KilledTimerInitial;
  }

  bool pressed(Control control) const { return controlStates[control]; }

  bool pressedOnce(Control control) {
    bool state = controlStates[control];
    controlStates.set(control, false);
    return state;
  }

  void release(Control control) { controlStates.set(control, false); }

  void press(Control control) { controlStates.set(control, true); }

  void setControlState(Control control, bool state) {
    controlStates.set(control, state);
  }

  void toggleControlState(Control control) {
    controlStates.set(control, !controlStates[control]);
  }

  int minimapColor() const { return 129 + index * 4; }

  void beginRespawn(Game& game);
  void doRespawning(Game& game);
  void process(Game& game);
  void processWeapons(Game& game);
  void processPhysics(Game& game);
  void processMovement(Game& game);
  void processTasks(Game& game);
  void processAiming(Game& game);
  void processWeaponChange(Game& game);
  void processSteerables(Game& game);
  void fire(Game& game);
  void processSight(Game& game);
  void calculateReactionForce(Game& game, int newX, int newY, int dir);
  void initWeapons(Game& game);
  int angleFrame() const;

  fixedvec pos, vel;

  gvl::ivec2 logicRespawn;

  int hotspotX, hotspotY;  // Hotspots for laser, laser sight, etc.
  fixed aimingAngle, aimingSpeed;

  bool ableToJump, ableToDig;  // The previous state of some keys
  bool keyChangePressed;
  bool movable;

  bool animate;         // Should the worm be animated?
  bool visible;         // Is the worm visible?
  bool ready;           // Is the worm ready to play?
  bool flag;            // Does the worm have a flag?
  bool makeSightGreen;  // Changes the sight color
  int health;           // Health left
  int lives;            // lives left
  int kills;            // Kills made

  int timer;        // Timer for GOT
  int killedTimer;  // Time until worm respawns
  static constexpr int KilledTimerInitial = 150;
  int currentFrame;

  int flags;  // How many flags does this worm have?

  Ninjarope ninjarope;

  int currentWeapon;    // The selected weapon
  int lastKilledByIdx;  // What worm that last killed this worm
  int fireCone;         // How much is left of the firecone
  int leaveShellTimer;  // Time until next shell drop

  std::shared_ptr<WormSettings> settings;  // !CLONING
  int index;                               // 0 or 1

  std::shared_ptr<WormAI> ai;

  int reacts[4];
  WormWeapon weapons[NUM_WEAPONS];
  int direction;
  ControlState controlStates;
  ControlState prevControlStates;

  // Temporary state for steerables
  int steerableSumX, steerableSumY, steerableCount;

  // which X coordinate to display stats at for this worm
  int statsX;

  // Data for LocalController
  ControlState cleanControlStates;  // This contains the real state of real and
                                    // extended controls
};

bool checkForWormHit(Game& game, int x, int y, int dist, Worm* ownWorm);
bool checkForSpecWormHit(Game& game, int x, int y, int dist, Worm& w);
int sqrVectorLength(int x, int y);
