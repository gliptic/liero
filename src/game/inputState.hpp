#pragma once

#include "state.hpp"

#include <cstdint>
#include <functional>
#include <string>

// Text input state. Draws the input field over the frozen screen and
// processes key/text events.
struct InputStringState : AppState {
  using Callback = std::function<void(bool accepted, std::string const& result)>;

  InputStringState(std::string initial, std::size_t maxLen, int x, int y, int (*filter)(int),
                   std::string prefix, bool centered, Callback callback);

  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;
  // Overlay so the menu beneath repaints each frame; frozenScreen
  // only captures the background, not the live menu items.
  bool isOverlay() const override { return true; }

 private:
  std::string buffer_;
  std::size_t maxLen_;
  int x_, y_;
  int (*filter_)(int);
  std::string prefix_;
  bool centered_;
  Callback callback_;
  bool done_ = false;
  bool accepted_ = false;
};

// Key binding state. Waits for a key/joystick press and returns via callback.
struct WaitForKeyState : AppState {
  using Callback = std::function<void(uint32_t key, bool isGamepad)>;

  // If extended is false, only non-extended (DOS) keys are accepted.
  WaitForKeyState(bool extended, Callback callback);

  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;

 private:
  bool extended_;
  Callback callback_;
  uint32_t result_ = 0;
  bool isGamepadResult_ = false;
  bool done_ = false;
};

// Info box. Shows a message and waits for any key press.
struct InfoBoxState : AppState {
  using DismissCallback = std::function<void()>;

  InfoBoxState(std::string text, int x, int y, bool clearScreen,
               DismissCallback onDismiss = nullptr);

  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;

 private:
  std::string text_;
  int x_, y_;
  bool clearScreen_;
  bool done_ = false;
  DismissCallback onDismiss_;
};
