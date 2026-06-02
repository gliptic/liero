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
          if (!buffer_.empty()) buffer_.erase(buffer_.size() - 1);
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
      int k = Utf8ToDos(ev.text.text);
      if (k && buffer_.size() < maxLen_ && (!filter_ || (k = filter_(k)))) {
        buffer_ += char(k);
      }
      break;
    }

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
  std::string str = prefix_ + buffer_ + '_';

  Font& font = gfx->common->font;
  int width = font.GetDims(str);
  int adjust = centered_ ? width / 2 : 0;
  int clr_x = x_ - 10 - adjust;

  BlitImageNoKeyColour(gfx->play_renderer.bmp, &gfx->frozen_screen.GetPixel(clr_x, y_), clr_x, y_,
                       clr_x + 10 + width, 8, gfx->frozen_screen.pitch);

  DrawRoundedBox(gfx->play_renderer.bmp, x_ - 2 - adjust, y_, 0, 7, width);
  font.DrawString(gfx->play_renderer.bmp, str, x_ - adjust, y_ + 1, 50);
}

// --- WaitForKeyState ---

WaitForKeyState::WaitForKeyState(bool extended, Callback callback)
    : extended_(extended), callback_(std::move(callback)) {}

void WaitForKeyState::Enter() {}

void WaitForKeyState::HandleEvent(SDL_Event& ev) {
  gfx->ProcessEvent(ev);

  switch (ev.type) {
    case SDL_EVENT_KEY_DOWN: {
      uint32_t k = SDLToDOSKey(ev.key.scancode);
      if (extended_ || !IsExtendedKey(k)) {
        result_ = k;
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
  std::string text = "PRESS A KEY";
  int height;
  int width = gfx->common->font.GetDims(text, &height);

  int cx = 160 - width / 2 - 2;
  int cy = 100 - height / 2 - 2;

  DrawRoundedBox(gfx->play_renderer.bmp, cx, cy, 0, height + 1, width + 1);
  gfx->common->font.DrawString(gfx->play_renderer.bmp, text, cx + 2, cy + 2, 50);
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

  if (ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) done_ = true;
}

bool InfoBoxState::Update() {
  if (done_) {
    gfx->ClearKeys();
    if (clearScreen_) Fill(gfx->play_renderer.bmp, 0);
    // onDismiss_ runs before this state is popped. It may call
    // scheduleReplaceTop() on the StateStack to chain into another
    // state; that takes precedence over the normal pop because the
    // stack checks pendingReplace_ before keepRunning.
    if (onDismiss_) onDismiss_();
    return false;
  }
  return true;
}

void InfoBoxState::Draw() {
  if (clearScreen_) {
    gfx->play_renderer.pal = gfx->common->exepal;
    Fill(gfx->play_renderer.bmp, 0);
  }

  int height;
  int width = gfx->common->font.GetDims(text_, &height);

  int cx = x_ - width / 2 - 2;
  int cy = y_ - height / 2 - 2;

  DrawRoundedBox(gfx->play_renderer.bmp, cx, cy, 0, height + 1, width + 1);
  gfx->common->font.DrawString(gfx->play_renderer.bmp, text_, cx + 2, cy + 2, 6);
}
