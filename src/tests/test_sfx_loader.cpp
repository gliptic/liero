#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

#include "common.hpp"
#include "io/stream.hpp"
#include "reader.hpp"
#include "tc_tool/common_exereader.hpp"

namespace {

void PutU16le(std::vector<uint8_t>& buf, uint16_t v) {
  buf.push_back(v & 0xff);
  buf.push_back((v >> 8) & 0xff);
}

void PutU32le(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(v & 0xff);
  buf.push_back((v >> 8) & 0xff);
  buf.push_back((v >> 16) & 0xff);
  buf.push_back((v >> 24) & 0xff);
}

void PutName8(std::vector<uint8_t>& buf, char const* name) {
  uint8_t pad[8] = {0};
  std::size_t n = std::strlen(name);
  if (n > 8) n = 8;
  std::memcpy(pad, name, n);
  buf.insert(buf.end(), pad, pad + 8);
}

}  // namespace

// Regression for issue #44 — a disabled (length-0) sample in the middle of
// the table must keep its slot so indices of entries after it don't shift.
TEST_CASE("loadSfx preserves disabled middle slots", "[sfx_loader]") {
  // Three samples: "first" (4 bytes), "middle" (disabled, length 0),
  // "last" (4 bytes). Header is 2 + 3*16 = 50 bytes; audio data follows.
  std::vector<uint8_t> blob;
  PutU16le(blob, 3);

  uint32_t const kHeaderEnd = 2 + 3 * 16;

  PutName8(blob, "first");
  PutU32le(blob, kHeaderEnd);
  PutU32le(blob, 4);

  PutName8(blob, "middle");
  PutU32le(blob, 0);
  PutU32le(blob, 0);

  PutName8(blob, "last");
  PutU32le(blob, kHeaderEnd + 4);
  PutU32le(blob, 4);

  uint8_t first_data[4] = {1, 2, 3, 4};
  uint8_t last_data[4] = {5, 6, 7, 8};
  blob.insert(blob.end(), first_data, first_data + 4);
  blob.insert(blob.end(), last_data, last_data + 4);

  io::MemReader mem(blob);
  ReaderFile rf(mem);

  std::vector<SfxSample> sounds;
  REQUIRE_NOTHROW(LoadSfx(sounds, rf));

  REQUIRE(sounds.size() == 3);

  REQUIRE(sounds[0].name == "first");
  REQUIRE(sounds[1].name == "middle");
  REQUIRE(sounds[2].name == "last");

  REQUIRE(sounds[0].sound != nullptr);
  REQUIRE(sounds[1].sound == nullptr);
  REQUIRE(sounds[2].sound != nullptr);
}
