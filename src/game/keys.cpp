#include <SDL3/SDL.h>
#include <cassert>
#include <cstddef>
#include <map>

static std::map<int, int> sdl_to_dos_scan_codes;

SDL_Scancode const kZ = SDL_SCANCODE_UNKNOWN;
static SDL_Scancode liero_to_sdl_keys[] = {
    SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
    SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,
    SDL_SCANCODE_0,
    /* 0x0c: */
    SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y,
    SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET,
    SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RETURN, SDL_SCANCODE_LCTRL, SDL_SCANCODE_A,
    SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J,
    SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE,
    SDL_SCANCODE_GRAVE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_Z, SDL_SCANCODE_X,
    SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M,
    /* 0x33: */
    SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_CAPSLOCK,
    SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
    SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10,
    /* 0x45: */
    SDL_SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_SCROLLLOCK, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8,
    SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5,
    SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2,
    SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_NONUSBACKSLASH, SDL_SCANCODE_F11, SDL_SCANCODE_F12,

    kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ,
    kZ, kZ, kZ,                                                        // 27 zeroes
    SDL_SCANCODE_KP_ENTER,                                             // Enter (Pad)
    SDL_SCANCODE_RCTRL,                                                // Right Ctrl
    kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ,                    // 12 zeroes
    SDL_SCANCODE_PRINTSCREEN, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ,  // 10 zeroes
    SDL_SCANCODE_KP_DIVIDE,                                            // / (Pad)
    kZ, SDL_SCANCODE_PRINTSCREEN,
    SDL_SCANCODE_RALT,                                       // Right Alt
    kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ, kZ,  // 14 zeroes
    SDL_SCANCODE_HOME,                                       // Home
    SDL_SCANCODE_UP,                                         // Up
    SDL_SCANCODE_PAGEUP,                                     // Page Up
    kZ,
    SDL_SCANCODE_LEFT,  // Left
    kZ,
    SDL_SCANCODE_RIGHT,  // Right
    kZ,
    SDL_SCANCODE_END,       // End
    SDL_SCANCODE_DOWN,      // Down
    SDL_SCANCODE_PAGEDOWN,  // Page Down
    SDL_SCANCODE_INSERT,    // Insert
    SDL_SCANCODE_DELETE,    // Delete
    kZ, kZ, kZ, kZ, kZ      // 5 zeroes
};

uint32_t const kMaxScanCodes = sizeof(liero_to_sdl_keys) / sizeof(*liero_to_sdl_keys);

void InitKeys() {
  for (std::size_t i = 0; i < kMaxScanCodes; ++i) {
    if (liero_to_sdl_keys[i] != SDL_SCANCODE_UNKNOWN) {
      sdl_to_dos_scan_codes[liero_to_sdl_keys[i]] = static_cast<int>(i);
    }
  }
}

uint32_t SDLToDOSKey(SDL_Scancode scancode) {
  auto const kI = sdl_to_dos_scan_codes.find(static_cast<uint32_t>(scancode));
  if (kI != sdl_to_dos_scan_codes.end()) {
    return kI->second;
  }
  return 89;
}

uint32_t SDLToDOSKey(SDL_Scancode scancode, SDL_Keymod /*mod*/) {
  uint32_t const kEy = SDLToDOSKey(scancode);

  if (kEy >= 177) {  // Liero doesn't have keys >= 177
    return 89;       // Arbitrarily translate it to 89
  }
  return kEy;
}
