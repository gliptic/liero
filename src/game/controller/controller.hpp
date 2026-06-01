#pragma once

struct Level;
struct Game;
struct Renderer;

struct Controller {
  virtual ~Controller() {}

  // Returns true if this controller is controlling a replay, false if it is
  // an actual match
  virtual bool isReplay() { return false; };

  // Called when a key event is forwarded to the controller
  virtual void onKey(int key, bool state) = 0;

  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  virtual void unfocus() = 0;

  // Called when the controller gets focus.
  virtual void focus() = 0;

  virtual bool process() = 0;

  virtual void draw(Renderer& renderer, bool useSpectatorViewports) = 0;

  // Returns true if the game is still running. The menu should check this to decide whether to show
  // the resume option.
  virtual bool running() = 0;

  // Notify the controller that any underlying session it depended on
  // is gone (peer left, socket closed, etc.) so it can stop reporting
  // itself as resumable. Default no-op for single-player controllers.
  virtual void markUnresumable() {}

  virtual Level* currentLevel() = 0;

  virtual Game* currentGame() = 0;

  // Game whose statsRecorder holds the post-match player-facing stats.
  // For single-player and replay this is the live game. For rollback
  // multiplayer it's the shadow Game that follows confirmed frames,
  // because the live game's processFrame fires speculatively and would
  // over-count (the live recorder is intentionally a no-op).
  virtual Game* statsGame() { return currentGame(); }

  virtual void swapLevel(Level& newLevel) = 0;
};
