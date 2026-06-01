#include "common.hpp"

#include <map>
#include <sstream>
#include <string>
#include "common_model.hpp"
#include "console.hpp"
#include "filesystem.hpp"
#include "gfx/blit.hpp"
#include "io/coding.hpp"
#include "io/stream.hpp"
#include "rand.hpp"
#include "worm.hpp"

int Common::fireConeOffset[FIRE_CONE_OFFSET_DIRECTION][FIRE_CONE_OFFSET_ANGLE_FRAME]
                          [FIRE_CONE_OFFSET_XY] = {
                              {{-3, 1}, {-4, 0}, {-4, -2}, {-4, -4}, {-3, -5}, {-2, -6}, {0, -6}},
                              {{3, 1}, {4, 0}, {4, -2}, {4, -4}, {3, -5}, {2, -6}, {0, -6}},
};

int stoneTab[3][4] = {{98, 60, 61, 62}, {63, 75, 85, 86}, {89, 90, 97, 96}};

char const* Texts::keyNames[177] = {
    "",
    "Esc",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "+",
    "`",
    "Backspace",
    "Tab",
    "Q",
    "W",
    "E",
    "R",
    "T",
    "Y",
    "U",
    "I",
    "O",
    "P",
    "Å",
    "^",
    "Enter",
    "Left Crtl",
    "A",
    "S",
    "D",
    "F",
    "G",
    "H",
    "J",
    "K",
    "L",
    "Ö",
    "Ä",
    "½",
    "Left Shift",
    "'",
    "Z",
    "X",
    "C",
    "V",
    "B",
    "N",
    "M",
    ",",
    ".",
    "-",
    "Right Shift",
    "* (Pad)",
    "Left Alt",
    "",
    "Caps Lock",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "Num Lock",
    "Scroll Lock",
    "7 (Pad)",
    "8 (Pad)",
    "9 (Pad)",
    "- (Pad)",
    "4 (Pad)",
    "5 (Pad)",
    "6 (Pad)",
    "+ (Pad)",
    "1 (Pad)",
    "2 (Pad)",
    "3 (Pad)",
    "0 (Pad)",
    ", (Pad)",
    "",
    "",
    "<",
    "F11",
    "F12",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Enter (Pad)",
    "Right Ctrl",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Print Screen",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "/ (Pad)",
    "",
    "Print Screen",
    "Right Alt",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Home",
    "Up",
    "Page Up",
    "",
    "Left",
    "",
    "Right",
    "",
    "End",
    "Down",
    "Page Down",
    "Insert",
    "Delete",
    "",
    "",
    "",
    "",
    "",
};

Texts::Texts() {
  gameModes[Settings::GameModes::GMKillEmAll] = "Kill'em All";
  gameModes[Settings::GameModes::GMGameOfTag] = "Game of Tag";
  gameModes[Settings::GameModes::GMHoldazone] = "Holdazone";
  gameModes[Settings::GameModes::GMScalesOfJustice] = "Scales of Justice";

  onoff[0] = "OFF";
  onoff[1] = "ON";

  controllers[0] = "Human";
  controllers[1] = "CPU";
  controllers[2] = "AI";

  inputDevices[0] = "Keyboard";
  inputDevices[1] = "Joystick 1";
  inputDevices[2] = "Joystick 2";

  weapStates[0] = "Menu";
  weapStates[1] = "Bonus";
  weapStates[2] = "Banned";

  copyrightBarFormat = 64;
}

void Common::drawTextSmall(Bitmap& scr, char const* str, int x, int y) {
  for (; *str; ++str) {
    unsigned char c = *str - 'A';

    if (c < 26) {
      blitImage(scr, textSprites[c], x, y);
    }

    x += 4;
  }
}

#define CHECK(c) \
  if (!(c)) goto fail

int readSpriteTga(io::Reader& r, int destImageWidth, int destImageHeight, int destCount,
                  uint8_t* data, Palette* pal) {
  auto idLen = r.get();
  CHECK(r.get() == 1);
  CHECK(r.get() == 1);

  // Palette spec
  CHECK(io::read_uint16_le(r) == 0);
  CHECK(io::read_uint16_le(r) == 256);
  CHECK(r.get() == 24);

  int imageWidth, imageHeight;

  CHECK(io::read_uint16_le(r) == 0);
  CHECK(io::read_uint16_le(r) == 0);

  imageWidth = io::read_uint16_le(r);
  imageHeight = io::read_uint16_le(r);
  CHECK(r.get() == 8);
  CHECK(r.get() == 0);

  r.try_skip(idLen);  // Skip ID

  // TODO: Support more sprites?
  CHECK(imageWidth == destImageWidth);
  CHECK(imageHeight == destImageHeight);

  if (pal) {
    for (auto& entry : pal->entries) {
      entry.b = r.get() >> 2;
      entry.g = r.get() >> 2;
      entry.r = r.get() >> 2;
    }
  } else {
    r.try_skip(256 * 3);  // Ignore palette
  }

  // Bottom to top
  for (std::size_t y = (std::size_t)imageHeight; y-- > 0;) {
    auto* src = &data[y * imageWidth];
    r.get((uint8_t*)src, imageWidth);
  }

  return 1;

fail:
  return 0;
}

int readSpriteTga(io::Reader& r, SpriteSet& ss, Palette* pal) {
  return readSpriteTga(r, ss.width, ss.count * ss.height, ss.count, &ss.data[0], pal);
}

#undef CHECK

inline uint32_t quad(char a, char b, char c, char d) {
  return (uint32_t)a + ((uint32_t)b << 8) + ((uint32_t)c << 16) + ((uint32_t)d << 24);
}

void Common::load(FsNode node) {
  {
    auto textReader_ptr = (node / "tc.cfg").toReader();
    io::Reader& textReader = *textReader_ptr;
    // Read entire content into a string for istringstream
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(textReader.get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    loadTcConfig(*this, is);
  }

  for (auto& s : sounds) {
    auto dir = node / "sounds";

    auto wavNode = dir / (s.name + ".wav");
    if (!wavNode.exists()) {
      // Missing WAV on disk: keep the slot (preserving stable indices for
      // siblings) but leave sound == nullptr so play paths treat it as a
      // silent no-op. Matches the disabled-slot behavior of tc_tool's
      // loadSfx (see issue #44).
      Console::writeWarning("Sound file missing, slot will be silent: " + s.name + ".wav");
      continue;
    }
    auto r_ptr = wavNode.toReader();
    io::Reader& r = *r_ptr;

    if (io::read_uint32_le(r) == quad('R', 'I', 'F', 'F')) {
      std::size_t roundedSize = io::read_uint32_le(r) + 8;

      (void)roundedSize;  // Ignore

      if (io::read_uint32_le(r) == quad('W', 'A', 'V', 'E') &&
          io::read_uint32_le(r) == quad('f', 'm', 't', ' ') && io::read_uint32_le(r) == 16 &&
          io::read_uint16_le(r) == 1 && io::read_uint16_le(r) == 1 &&
          io::read_uint32_le(r) == 22050 && io::read_uint32_le(r) == 22050 * 1 * 1 &&
          io::read_uint16_le(r) == 1 * 1 && io::read_uint16_le(r) == 8 &&
          io::read_uint32_le(r) == quad('d', 'a', 't', 'a')) {
        std::size_t dataSize = io::read_uint32_le(r);

        s.originalData.resize(dataSize);

        for (auto& z : s.originalData) z = r.get() - 128;

        s.sound = sfx_new_sound(dataSize * 2);

        s.createSound();
      }
    }
  }

  {
    auto dir = node / "sprites";

    largeSprites.allocate(16, 16, 110);
    smallSprites.allocate(7, 7, 130);
    textSprites.allocate(4, 4, 26);

    {
      auto r_ptr = (dir / "small.tga").toReader();
      io::Reader& r = *r_ptr;

      readSpriteTga(r, smallSprites, &exepal);
    }

    {
      auto r_ptr = (dir / "large.tga").toReader();
      io::Reader& r = *r_ptr;
      readSpriteTga(r, largeSprites, 0);
    }

    {
      auto r_ptr = (dir / "text.tga").toReader();
      io::Reader& r = *r_ptr;
      readSpriteTga(r, textSprites, 0);
    }

    {
      auto r_ptr = (dir / "font.tga").toReader();
      io::Reader& r = *r_ptr;

      std::vector<uint8_t> data(font.chars.size() * 7 * 8, 10);

      readSpriteTga(r, 7, (int)font.chars.size() * 8, (int)font.chars.size(), &data[0], 0);

      for (std::size_t i = 0; i < font.chars.size(); ++i) {
        Font::Char& ch = font.chars[i];
        uint8_t* dest = &data[i * 7 * 8];

        ch.width = 0;

        for (std::size_t y = 0; y < 8; ++y)
          for (std::size_t x = 0; x < 7; ++x) {
            auto p = dest[y * 7 + x];
            if (p == 0) {
              ch.data[y * 7 + x] = 0;
            } else if (p == 50) {
              ch.data[y * 7 + x] = 8;
            } else {
              ch.width = (int)x;
              break;
            }
          }
      }
    }
  }

  for (auto& w : weapons) {
    auto dir = node / "weapons";

    auto wReader_ptr = (dir / (w.idStr + ".cfg")).toReader();
    io::Reader& wReader = *wReader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(wReader.get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    loadWeaponConfig(*this, w, is);
  }

  for (auto& w : nobjectTypes) {
    auto dir = node / "nobjects";

    auto nReader_ptr = (dir / (w.idStr + ".cfg")).toReader();
    io::Reader& nReader = *nReader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(nReader.get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    loadNObjectConfig(*this, w, is);
  }

  for (auto& w : sobjectTypes) {
    auto dir = node / "sobjects";

    auto sReader_ptr = (dir / (w.idStr + ".cfg")).toReader();
    io::Reader& sReader = *sReader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(sReader.get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    loadSObjectConfig(*this, w, is);
  }

  precompute();
}

void Common::precompute() {
  weapOrder.resize(weapons.size());
  for (int i = 0; i < (int)weapons.size(); ++i) {
    weapOrder[i] = i;
    weapons[i].id = i;
  }

  std::sort(weapOrder.begin(), weapOrder.end(),
            [&](int a, int b) { return this->weapons[a].name < this->weapons[b].name; });

  for (int i = 0; i < (int)nobjectTypes.size(); ++i) {
    nobjectTypes[i].id = i;
  }

  for (int i = 0; i < (int)sobjectTypes.size(); ++i) {
    sobjectTypes[i].id = i;
  }

  // Precompute sprites
  wormSprites.allocate(16, 16, 2 * 2 * 21);

  for (int i = 0; i < 21; ++i) {
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        PalIdx pix = (largeSprites.spritePtr(16 + i) + y * 16)[x];

        (wormSprite(i, 1, 0) + y * 16)[x] = pix;
        if (x == 15)
          (wormSprite(i, 0, 0) + y * 16)[15] = 0;
        else
          (wormSprite(i, 0, 0) + y * 16)[14 - x] = pix;

        if (pix >= 30 && pix <= 34) pix += 9;  // Change worm color

        (wormSprite(i, 1, 1) + y * 16)[x] = pix;

        if (x == 15)
          (wormSprite(i, 0, 1) + y * 16)[15] = 0;  // A bit haxy, but works
        else
          (wormSprite(i, 0, 1) + y * 16)[14 - x] = pix;
      }
  }

  fireConeSprites.allocate(16, 16, 2 * 7);

  for (int i = 0; i < 7; ++i) {
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        PalIdx pix = (largeSprites.spritePtr(9 + i) + y * 16)[x];

        (fireConeSprite(i, 1) + y * 16)[x] = pix;

        if (x == 15)
          (fireConeSprite(i, 0) + y * 16)[15] = 0;
        else
          (fireConeSprite(i, 0) + y * 16)[14 - x] = pix;
      }
  }
}

Common::Common() {}

std::string Common::guessName() const {
  std::string const& cp = S[SCopyright2];
  auto p = cp.find('(');
  if (p == std::string::npos) p = cp.size();

  while (p > 0 && cp[p - 1] == ' ') --p;

  return cp.substr(0, p);
}

int Common::soundIndex(std::string_view name) const {
  for (std::size_t i = 0; i < sounds.size(); ++i) {
    if (sounds[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

void SfxSample::createSound() {
  if (originalData.empty()) return;

  std::vector<int16_t>& samples = sfx_sound_data(sound);
  samples.clear();

  int prev = static_cast<int8_t>(originalData[0]) * 30;
  samples.push_back(prev);

  for (std::size_t j = 1; j < originalData.size(); ++j) {
    int cur = static_cast<int8_t>(originalData[j]) * 30;
    samples.push_back((prev + cur) / 2);
    samples.push_back(cur);
    prev = cur;
  }

  samples.push_back(prev);
}
