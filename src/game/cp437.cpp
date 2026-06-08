#include "cp437.hpp"

#include <array>

namespace cp437 {

namespace {

// Standard CP437 → Unicode mapping for the high half (0x80–0xFF).
// Source: Unicode CP437.TXT (Microsoft).
constexpr char32_t kHighHalf[128] = {
    // 0x80
    0x00C7,
    0x00FC,
    0x00E9,
    0x00E2,
    0x00E4,
    0x00E0,
    0x00E5,
    0x00E7,
    0x00EA,
    0x00EB,
    0x00E8,
    0x00EF,
    0x00EE,
    0x00EC,
    0x00C4,
    0x00C5,
    // 0x90
    0x00C9,
    0x00E6,
    0x00C6,
    0x00F4,
    0x00F6,
    0x00F2,
    0x00FB,
    0x00F9,
    0x00FF,
    0x00D6,
    0x00DC,
    0x00A2,
    0x00A3,
    0x00A5,
    0x20A7,
    0x0192,
    // 0xA0
    0x00E1,
    0x00ED,
    0x00F3,
    0x00FA,
    0x00F1,
    0x00D1,
    0x00AA,
    0x00BA,
    0x00BF,
    0x2310,
    0x00AC,
    0x00BD,
    0x00BC,
    0x00A1,
    0x00AB,
    0x00BB,
    // 0xB0
    0x2591,
    0x2592,
    0x2593,
    0x2502,
    0x2524,
    0x2561,
    0x2562,
    0x2556,
    0x2555,
    0x2563,
    0x2551,
    0x2557,
    0x255D,
    0x255C,
    0x255B,
    0x2510,
    // 0xC0
    0x2514,
    0x2534,
    0x252C,
    0x251C,
    0x2500,
    0x253C,
    0x255E,
    0x255F,
    0x255A,
    0x2554,
    0x2569,
    0x2566,
    0x2560,
    0x2550,
    0x256C,
    0x2567,
    // 0xD0
    0x2568,
    0x2564,
    0x2565,
    0x2559,
    0x2558,
    0x2552,
    0x2553,
    0x256B,
    0x256A,
    0x2518,
    0x250C,
    0x2588,
    0x2584,
    0x258C,
    0x2590,
    0x2580,
    // 0xE0
    0x03B1,
    0x00DF,
    0x0393,
    0x03C0,
    0x03A3,
    0x03C3,
    0x00B5,
    0x03C4,
    0x03A6,
    0x0398,
    0x03A9,
    0x03B4,
    0x221E,
    0x03C6,
    0x03B5,
    0x2229,
    // 0xF0
    0x2261,
    0x00B1,
    0x2265,
    0x2264,
    0x2320,
    0x2321,
    0x00F7,
    0x2248,
    0x00B0,
    0x2219,
    0x00B7,
    0x221A,
    0x207F,
    0x00B2,
    0x25A0,
    0x00A0,
};

void AppendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

}  // namespace

char32_t ByteToUnicode(uint8_t b) {
  if (b < 0x80) return b;
  return kHighHalf[b - 0x80];
}

int UnicodeToByte(char32_t cp) {
  if (cp < 0x80) return static_cast<int>(cp);
  for (int i = 0; i < 128; ++i) {
    if (kHighHalf[i] == cp) return i + 0x80;
  }
  return -1;
}

char32_t Utf8DecodeNext(char const* str, std::size_t len, std::size_t& i) {
  if (i >= len) return 0xFFFD;
  auto const kC = static_cast<unsigned char>(str[i]);
  if (kC < 0x80) {
    ++i;
    return kC;
  }
  auto cont = [&](std::size_t k) -> int {
    if (i + k >= len) return -1;
    auto const kB = static_cast<unsigned char>(str[i + k]);
    if ((kB & 0xC0) != 0x80) return -1;
    return kB & 0x3F;
  };
  if ((kC & 0xE0) == 0xC0) {
    int const kB1 = cont(1);
    if (kB1 < 0) {
      ++i;
      return 0xFFFD;
    }
    char32_t const kCp = ((kC & 0x1F) << 6) | kB1;
    i += 2;
    return kCp < 0x80 ? 0xFFFD : kCp;  // reject overlong
  }
  if ((kC & 0xF0) == 0xE0) {
    int const kB1 = cont(1);
    int const kB2 = cont(2);
    if (kB1 < 0 || kB2 < 0) {
      ++i;
      return 0xFFFD;
    }
    char32_t const kCp = ((kC & 0x0F) << 12) | (kB1 << 6) | kB2;
    i += 3;
    return kCp < 0x800 ? 0xFFFD : kCp;
  }
  if ((kC & 0xF8) == 0xF0) {
    int const kB1 = cont(1);
    int const kB2 = cont(2);
    int const kB3 = cont(3);
    if (kB1 < 0 || kB2 < 0 || kB3 < 0) {
      ++i;
      return 0xFFFD;
    }
    char32_t const kCp = ((kC & 0x07) << 18) | (kB1 << 12) | (kB2 << 6) | kB3;
    i += 4;
    return kCp < 0x10000 ? 0xFFFD : kCp;
  }
  ++i;
  return 0xFFFD;
}

std::string Cp437BytesToUtf8(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for (unsigned char const kC : in) AppendUtf8(out, ByteToUnicode(kC));
  return out;
}

}  // namespace cp437
