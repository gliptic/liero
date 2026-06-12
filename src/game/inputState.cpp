#include "inputState.hpp"

#include "common.hpp"
#include "gfx.hpp"
#include "keys.hpp"
#include "mixer/player.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>

// --- InputStringState ---

InputStringState::InputStringState(std::string initial, std::size_t max_len, int x, int y,
                                   int (*filter)(int), std::string prefix, bool centered,
                                   Callback callback)
    : buffer_(std::move(initial)),
      maxLen_(max_len),
      x_(x),
      y_(y),
      filter_(filter),
      prefix_(std::move(prefix)),
      centered_(centered),
      callback_(std::move(callback)) {}

void InputStringState::Enter() { SDL_StartTextInput(gfx->sdl_window); }

void InputStringState::HandleEvent(SDL_Event& ev) {
  gfx->ProcessEvent(ev);

  switch (ev.type) {
    case SDL_EVENT_KEY_DOWN:
      switch (ev.key.scancode) {
        case SDL_SCANCODE_BACKSPACE:
          if (!buffer_.empty()) {
            buffer_.erase(buffer_.size() - 1);
          }
          break;

        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
          accepted_ = true;
          done_ = true;
          break;

        case SDL_SCANCODE_ESCAPE:
          accepted_ = false;
          done_ = true;
          break;

        default:
          break;
      }
      break;

    case SDL_EVENT_TEXT_INPUT: {
      // NOLINTNEXTLINE(bugprone-signed-char-misuse, cert-str34-c) — ev.text.text is plain `char[]` (SDL ABI); Utf8ToDos handles the conversion semantics internally.
      int k = Utf8ToDos(ev.text.text);
      // NOLINTNEXTLINE(bugprone-assignment-in-if-condition) — short-circuiting filter mutates k; refactoring degrades clarity.
      if (k && buffer_.size() < maxLen_ && (!filter_ || (k = filter_(k)))) {
        buffer_ += static_cast<char>(k);
      }
      break;
    }

    // NOLINTNEXTLINE(bugprone-branch-clone) — explicit no-op for SDL_EVENT_TEXT_EDITING documents the choice not to handle IME composition.
    case SDL_EVENT_TEXT_EDITING:
      // No complex IME support
      break;

    default:
      break;
  }
}

bool InputStringState::Update() {
  if (done_) {
    SDL_StopTextInput(gfx->sdl_window);
    g_sound_player->Play(gfx->common->sound_hook[SoundMenuSelect]);
    gfx->ClearKeys();
    callback_(accepted_, buffer_);
    return false;
  }
  return true;
}

void InputStringState::Draw() {
  std::string const kStr = prefix_ + buffer_ + '_';

  Font& font = gfx->common->font;
  int const kWidth = font.GetDims(kStr);
  int const kAdjust = centered_ ? kWidth / 2 : 0;
  int const kClrX = x_ - 10 - kAdjust;

  BlitBitmap(gfx->play_renderer.bmp, gfx->frozen_screen, kClrX, y_, kClrX + 10 + kWidth, 8);

  DrawRoundedBox(gfx->play_renderer.bmp, x_ - 2 - kAdjust, y_, 0, 7, kWidth);
  font.DrawString(gfx->play_renderer.bmp, kStr, x_ - kAdjust, y_ + 1, 50);
}

// --- WaitForKeyState ---

WaitForKeyState::WaitForKeyState(bool extended, Callback callback)
    : extended_(extended), callback_(std::move(callback)) {}

void WaitForKeyState::Enter() {}

void WaitForKeyState::HandleEvent(SDL_Event& ev) {
  gfx->ProcessEvent(ev);

  switch (ev.type) {
    case SDL_EVENT_KEY_DOWN: {
      uint32_t const k_ = SDLToDOSKey(ev.key.scancode);
      if (extended_ || !IsExtendedKey(k_)) {
        result_ = k_;
        done_ = true;
      }
      break;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (ev.gaxis.value > kJoyAxisThreshold) {
        result_ = WormSettingsExtensions::GamepadAxisPositive(ev.gaxis.axis);
        isGamepadResult_ = true;
        done_ = true;
      } else if (ev.gaxis.value < -kJoyAxisThreshold) {
        result_ = WormSettingsExtensions::GamepadAxisNegative(ev.gaxis.axis);
        isGamepadResult_ = true;
        done_ = true;
      }
      break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      result_ = ev.gbutton.button;
      isGamepadResult_ = true;
      done_ = true;
      break;

    default:
      break;
  }
}

bool WaitForKeyState::Update() {
  if (done_) {
    gfx->ClearKeys();
    callback_(result_, isGamepadResult_);
    return false;
  }
  return true;
}

void WaitForKeyState::Draw() {
  std::string const kText = "PRESS A KEY";
  int height = 0;
  int const kWidth = gfx->common->font.GetDims(kText, &height);

  int const kCx = 160 - kWidth / 2 - 2;
  int const kCy = 100 - height / 2 - 2;

  DrawRoundedBox(gfx->play_renderer.bmp, kCx, kCy, 0, height + 1, kWidth + 1);
  gfx->common->font.DrawString(gfx->play_renderer.bmp, kText, kCx + 2, kCy + 2, 50);
}

// --- InfoBoxState ---

InfoBoxState::InfoBoxState(std::string text, int x, int y, bool clear_screen,
                           DismissCallback on_dismiss)
    : text_(std::move(text)),
      x_(x),
      y_(y),
      clearScreen_(clear_screen),
      onDismiss_(std::move(on_dismiss)) {}

void InfoBoxState::Enter() {}

void InfoBoxState::HandleEvent(SDL_Event& ev) {
  gfx->ProcessEvent(ev);

  if (ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    done_ = true;
  }
}

bool InfoBoxState::Update() {
  if (done_) {
    gfx->ClearKeys();
    if (clearScreen_) {
      Fill(gfx->play_renderer.bmp, 0);
    }
    // onDismiss_ runs before this state is popped. It may call
    // scheduleReplaceTop() on the StateStack to chain into another
    // state; that takes precedence over the normal pop because the
    // stack checks pendingReplace_ before keepRunning.
    if (onDismiss_) {
      onDismiss_();
    }
    return false;
  }
  return true;
}

void InfoBoxState::Draw() {
  if (clearScreen_) {
    gfx->play_renderer.pal = gfx->common->exepal;
    gfx->play_renderer.UpdatePal32();
    Fill(gfx->play_renderer.bmp, 0);
  }

  int height = 0;
  int const kWidth = gfx->common->font.GetDims(text_, &height);

  int const kCx = x_ - kWidth / 2 - 2;
  int const kCy = y_ - height / 2 - 2;

  DrawRoundedBox(gfx->play_renderer.bmp, kCx, kCy, 0, height + 1, kWidth + 1);
  gfx->common->font.DrawString(gfx->play_renderer.bmp, text_, kCx + 2, kCy + 2, 6);
}
