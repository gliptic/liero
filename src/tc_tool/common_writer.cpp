#include "game/common_model.hpp"
#include "game/filesystem.hpp"
#include "game/io/coding.hpp"
#include "game/io/stream.hpp"

#include <fstream>
#include <sstream>

void WriteSpriteTga(io::Writer& w, int image_width, int image_height, uint8_t* data, Palette& pal) {
  w.Put(0);
  w.Put(1);
  w.Put(1);

  // Palette spec
  io::WriteUint16Le(w, 0);
  io::WriteUint16Le(w, 256);
  w.Put(24);

  io::WriteUint16Le(w, 0);
  io::WriteUint16Le(w, 0);
  io::WriteUint16Le(w, image_width);
  io::WriteUint16Le(w, image_height);
  w.Put(8);  // Bits per pixel
  w.Put(0);  // Descriptor

  for (auto const& entry : pal.entries) {
    w.Put(entry.b << 2);
    w.Put(entry.g << 2);
    w.Put(entry.r << 2);
  }

  // Bottom to top
  for (std::size_t y = (std::size_t)image_height; y-- > 0;) {
    auto const* src = &data[y * image_width];
    w.Put((uint8_t const*)src, image_width);
  }
}

void WriteSpriteTga(io::Writer& w, SpriteSet& ss, Palette& pal) {
  WriteSpriteTga(w, ss.width, ss.count * ss.height, &ss.data[0], pal);
}

void CommonSave(Common& common, std::string const& path) {
  auto cfg_path = JoinPath(path, "tc.cfg");
  CreateDirectories(cfg_path);

  {
    std::ostringstream ss;
    SaveTcConfig(common, ss);
    io::FileWriter text_writer(cfg_path.c_str(), "wb");
    auto str = ss.str();
    for (char c : str) text_writer.Put(c);
  }

  for (auto& s : common.sounds) {
    std::string dir(JoinPath(path, "sounds/"));
    CreateDirectories(dir);

    io::FileWriter w(JoinPath(dir, s.name + ".wav").c_str(), "wb");

    auto rounded_size = (s.original_data.size() + 1) & ~1;

    w.Put((uint8_t const*)"RIFF", 4);
    io::WriteUint32Le(w, (uint32_t)rounded_size - 8);
    w.Put((uint8_t const*)"WAVE", 4);

    w.Put((uint8_t const*)"fmt ", 4);
    io::WriteUint32Le(w, 16);     // PCM header size
    io::WriteUint16Le(w, 1);      // PCM
    io::WriteUint16Le(w, 1);      // Mono
    io::WriteUint32Le(w, 22050);  // Sample rate
    io::WriteUint32Le(w, 22050 * 1 * 1);
    io::WriteUint16Le(w, 1 * 1);
    io::WriteUint16Le(w, 8);

    w.Put((uint8_t const*)"data", 4);
    io::WriteUint32Le(w, (uint32_t)s.original_data.size() * 1 * 1);  // Data size

    auto cur_size = s.original_data.size();

    for (auto& z : s.original_data) w.Put(z + 128);

    while (cur_size < rounded_size) {
      w.Put(0);  // Padding
      ++cur_size;
    }
  }

  {
    std::string dir(JoinPath(path, "sprites/"));
    CreateDirectories(dir);

    {
      io::FileWriter w(JoinPath(dir, "small.tga").c_str(), "wb");
      WriteSpriteTga(w, common.small_sprites, common.exepal);
    }

    {
      io::FileWriter w(JoinPath(dir, "large.tga").c_str(), "wb");
      WriteSpriteTga(w, common.large_sprites, common.exepal);
    }

    {
      io::FileWriter w(JoinPath(dir, "text.tga").c_str(), "wb");
      WriteSpriteTga(w, common.text_sprites, common.exepal);
    }

    {
      io::FileWriter w(JoinPath(dir, "font.tga").c_str(), "wb");

      std::vector<uint8_t> data(common.font.chars.size() * 7 * 8, 10);
      for (std::size_t i = 0; i < common.font.chars.size(); ++i) {
        Font::Char& ch = common.font.chars[i];
        uint8_t* dest = &data[i * 7 * 8];

        for (std::size_t y = 0; y < 8; ++y)
          for (std::size_t x = 0; x < ch.width; ++x) {
            dest[y * 7 + x] = ch.data[y * 7 + x] ? 50 : 0;
          }
      }
      WriteSpriteTga(w, 7, (int)common.font.chars.size() * 8, &data[0], common.exepal);
    }
  }

  for (auto& w : common.weapons) {
    std::string dir(JoinPath(path, "weapons/"));
    CreateDirectories(dir);

    std::ostringstream ss;
    SaveWeaponConfig(common, w, ss);
    io::FileWriter w_writer(JoinPath(dir, w.id_str + ".cfg").c_str(), "wb");
    auto str = ss.str();
    for (char c : str) w_writer.Put(c);
  }

  for (auto& w : common.nobject_types) {
    std::string dir(JoinPath(path, "nobjects/"));
    CreateDirectories(dir);

    std::ostringstream ss;
    SaveNObjectConfig(common, w, ss);
    io::FileWriter n_writer(JoinPath(dir, w.id_str + ".cfg").c_str(), "wb");
    auto str = ss.str();
    for (char c : str) n_writer.Put(c);
  }

  for (auto& w : common.sobject_types) {
    std::string dir(JoinPath(path, "sobjects/"));
    CreateDirectories(dir);

    std::ostringstream ss;
    SaveSObjectConfig(common, w, ss);
    io::FileWriter s_writer(JoinPath(dir, w.id_str + ".cfg").c_str(), "wb");
    auto str = ss.str();
    for (char c : str) s_writer.Put(c);
  }
}
