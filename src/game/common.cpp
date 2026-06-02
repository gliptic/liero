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

int Common::fire_cone_offset[FIRE_CONE_OFFSET_DIRECTION][FIRE_CONE_OFFSET_ANGLE_FRAME]
                            [FIRE_CONE_OFFSET_XY] = {
                                {{-3, 1}, {-4, 0}, {-4, -2}, {-4, -4}, {-3, -5}, {-2, -6}, {0, -6}},
                                {{3, 1}, {4, 0}, {4, -2}, {4, -4}, {3, -5}, {2, -6}, {0, -6}},
};

int stone_tab[3][4] = {{98, 60, 61, 62}, {63, 75, 85, 86}, {89, 90, 97, 96}};

char const* Texts::key_names[177] = {
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
  game_modes[Settings::GameModes::kGmKillEmAll] = "Kill'em All";
  game_modes[Settings::GameModes::kGmGameOfTag] = "Game of Tag";
  game_modes[Settings::GameModes::kGmHoldazone] = "Holdazone";
  game_modes[Settings::GameModes::kGmScalesOfJustice] = "Scales of Justice";

  onoff[0] = "OFF";
  onoff[1] = "ON";

  controllers[0] = "Human";
  controllers[1] = "CPU";
  controllers[2] = "AI";

  input_devices[0] = "Keyboard";
  input_devices[1] = "Joystick 1";
  input_devices[2] = "Joystick 2";

  weap_states[0] = "Menu";
  weap_states[1] = "Bonus";
  weap_states[2] = "Banned";

  copyright_bar_format = 64;
}

void Common::DrawTextSmall(Bitmap& scr, char const* str, int x, int y) {
  for (; *str; ++str) {
    unsigned char c = *str - 'A';

    if (c < 26) {
      BlitImage(scr, text_sprites[c], x, y);
    }

    x += 4;
  }
}

#define CHECK(c) \
  if (!(c)) goto fail

int ReadSpriteTga(io::Reader& r, int dest_image_width, int dest_image_height, int dest_count,
                  uint8_t* data, Palette* pal) {
  auto id_len = r.Get();
  CHECK(r.Get() == 1);
  CHECK(r.Get() == 1);

  // Palette spec
  CHECK(io::ReadUint16Le(r) == 0);
  CHECK(io::ReadUint16Le(r) == 256);
  CHECK(r.Get() == 24);

  int image_width, image_height;

  CHECK(io::ReadUint16Le(r) == 0);
  CHECK(io::ReadUint16Le(r) == 0);

  image_width = io::ReadUint16Le(r);
  image_height = io::ReadUint16Le(r);
  CHECK(r.Get() == 8);
  CHECK(r.Get() == 0);

  r.TrySkip(id_len);  // Skip ID

  // TODO: Support more sprites?
  CHECK(image_width == dest_image_width);
  CHECK(image_height == dest_image_height);

  if (pal) {
    for (auto& entry : pal->entries) {
      entry.b = r.Get() >> 2;
      entry.g = r.Get() >> 2;
      entry.r = r.Get() >> 2;
    }
  } else {
    r.TrySkip(256 * 3);  // Ignore palette
  }

  // Bottom to top
  for (std::size_t y = (std::size_t)image_height; y-- > 0;) {
    auto* src = &data[y * image_width];
    r.Get((uint8_t*)src, image_width);
  }

  return 1;

fail:
  return 0;
}

int ReadSpriteTga(io::Reader& r, SpriteSet& ss, Palette* pal) {
  return ReadSpriteTga(r, ss.width, ss.count * ss.height, ss.count, &ss.data[0], pal);
}

#undef CHECK

inline uint32_t Quad(char a, char b, char c, char d) {
  return (uint32_t)a + ((uint32_t)b << 8) + ((uint32_t)c << 16) + ((uint32_t)d << 24);
}

void Common::load(FsNode node) {
  {
    auto text_reader_ptr = (node / "tc.cfg").ToReader();
    io::Reader& text_reader = *text_reader_ptr;
    // Read entire content into a string for istringstream
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(text_reader.Get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    LoadTcConfig(*this, is);
  }

  for (auto& s : sounds) {
    auto dir = node / "sounds";

    auto wav_node = dir / (s.name + ".wav");
    if (!wav_node.Exists()) {
      // Missing WAV on disk: keep the slot (preserving stable indices for
      // siblings) but leave sound == nullptr so play paths treat it as a
      // silent no-op. Matches the disabled-slot behavior of tc_tool's
      // loadSfx (see issue #44).
      console::WriteWarning("Sound file missing, slot will be silent: " + s.name + ".wav");
      continue;
    }
    auto r_ptr = wav_node.ToReader();
    io::Reader& r = *r_ptr;

    if (io::ReadUint32Le(r) == Quad('R', 'I', 'F', 'F')) {
      std::size_t rounded_size = io::ReadUint32Le(r) + 8;

      (void)rounded_size;  // Ignore

      if (io::ReadUint32Le(r) == Quad('W', 'A', 'V', 'E') &&
          io::ReadUint32Le(r) == Quad('f', 'm', 't', ' ') && io::ReadUint32Le(r) == 16 &&
          io::ReadUint16Le(r) == 1 && io::ReadUint16Le(r) == 1 && io::ReadUint32Le(r) == 22050 &&
          io::ReadUint32Le(r) == 22050 * 1 * 1 && io::ReadUint16Le(r) == 1 * 1 &&
          io::ReadUint16Le(r) == 8 && io::ReadUint32Le(r) == Quad('d', 'a', 't', 'a')) {
        std::size_t data_size = io::ReadUint32Le(r);

        s.original_data.resize(data_size);

        for (auto& z : s.original_data) z = r.Get() - 128;

        s.sound = SfxNewSound(data_size * 2);

        s.CreateSound();
      }
    }
  }

  {
    auto dir = node / "sprites";

    large_sprites.Allocate(16, 16, 110);
    small_sprites.Allocate(7, 7, 130);
    text_sprites.Allocate(4, 4, 26);

    {
      auto r_ptr = (dir / "small.tga").ToReader();
      io::Reader& r = *r_ptr;

      ReadSpriteTga(r, small_sprites, &exepal);
    }

    {
      auto r_ptr = (dir / "large.tga").ToReader();
      io::Reader& r = *r_ptr;
      ReadSpriteTga(r, large_sprites, 0);
    }

    {
      auto r_ptr = (dir / "text.tga").ToReader();
      io::Reader& r = *r_ptr;
      ReadSpriteTga(r, text_sprites, 0);
    }

    {
      auto r_ptr = (dir / "font.tga").ToReader();
      io::Reader& r = *r_ptr;

      std::vector<uint8_t> data(font.chars.size() * 7 * 8, 10);

      ReadSpriteTga(r, 7, (int)font.chars.size() * 8, (int)font.chars.size(), &data[0], 0);

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

    auto w_reader_ptr = (dir / (w.id_str + ".cfg")).ToReader();
    io::Reader& w_reader = *w_reader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(w_reader.Get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    LoadWeaponConfig(*this, w, is);
  }

  for (auto& w : nobject_types) {
    auto dir = node / "nobjects";

    auto n_reader_ptr = (dir / (w.id_str + ".cfg")).ToReader();
    io::Reader& n_reader = *n_reader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(n_reader.Get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    LoadNObjectConfig(*this, w, is);
  }

  for (auto& w : sobject_types) {
    auto dir = node / "sobjects";

    auto s_reader_ptr = (dir / (w.id_str + ".cfg")).ToReader();
    io::Reader& s_reader = *s_reader_ptr;
    std::string content;
    try {
      for (;;) content.push_back(static_cast<char>(s_reader.Get()));
    } catch (std::runtime_error&) {
    }
    std::istringstream is(content);
    LoadSObjectConfig(*this, w, is);
  }

  Precompute();
}

void Common::Precompute() {
  weap_order.resize(weapons.size());
  for (int i = 0; i < (int)weapons.size(); ++i) {
    weap_order[i] = i;
    weapons[i].id = i;
  }

  std::sort(weap_order.begin(), weap_order.end(),
            [&](int a, int b) { return this->weapons[a].name < this->weapons[b].name; });

  for (int i = 0; i < (int)nobject_types.size(); ++i) {
    nobject_types[i].id = i;
  }

  for (int i = 0; i < (int)sobject_types.size(); ++i) {
    sobject_types[i].id = i;
  }

  // Precompute sprites
  worm_sprites.Allocate(16, 16, 2 * 2 * 21);

  for (int i = 0; i < 21; ++i) {
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        PalIdx pix = (large_sprites.SpritePtr(16 + i) + y * 16)[x];

        (WormSprite(i, 1, 0) + y * 16)[x] = pix;
        if (x == 15)
          (WormSprite(i, 0, 0) + y * 16)[15] = 0;
        else
          (WormSprite(i, 0, 0) + y * 16)[14 - x] = pix;

        if (pix >= 30 && pix <= 34) pix += 9;  // Change worm color

        (WormSprite(i, 1, 1) + y * 16)[x] = pix;

        if (x == 15)
          (WormSprite(i, 0, 1) + y * 16)[15] = 0;  // A bit haxy, but works
        else
          (WormSprite(i, 0, 1) + y * 16)[14 - x] = pix;
      }
  }

  fire_cone_sprites.Allocate(16, 16, 2 * 7);

  for (int i = 0; i < 7; ++i) {
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        PalIdx pix = (large_sprites.SpritePtr(9 + i) + y * 16)[x];

        (FireConeSprite(i, 1) + y * 16)[x] = pix;

        if (x == 15)
          (FireConeSprite(i, 0) + y * 16)[15] = 0;
        else
          (FireConeSprite(i, 0) + y * 16)[14 - x] = pix;
      }
  }
}

Common::Common() {}

std::string Common::GuessName() const {
  std::string const& cp = s[SCopyright2];
  auto p = cp.find('(');
  if (p == std::string::npos) p = cp.size();

  while (p > 0 && cp[p - 1] == ' ') --p;

  return cp.substr(0, p);
}

int Common::SoundIndex(std::string_view name) const {
  for (std::size_t i = 0; i < sounds.size(); ++i) {
    if (sounds[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

void SfxSample::CreateSound() {
  if (original_data.empty()) return;

  std::vector<int16_t>& samples = SfxSoundData(sound);
  samples.clear();

  int prev = static_cast<int8_t>(original_data[0]) * 30;
  samples.push_back(prev);

  for (std::size_t j = 1; j < original_data.size(); ++j) {
    int cur = static_cast<int8_t>(original_data[j]) * 30;
    samples.push_back((prev + cur) / 2);
    samples.push_back(cur);
    prev = cur;
  }

  samples.push_back(prev);
}
