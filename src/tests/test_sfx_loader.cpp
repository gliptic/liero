#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

#include "common.hpp"
#include "io/stream.hpp"
#include "reader.hpp"
#include "tc_tool/common_exereader.hpp"

namespace {

void put_u16le(std::vector<uint8_t>& buf, uint16_t v) {
  buf.push_back(v & 0xff);
  buf.push_back((v >> 8) & 0xff);
}

void put_u32le(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(v & 0xff);
  buf.push_back((v >> 8) & 0xff);
  buf.push_back((v >> 16) & 0xff);
  buf.push_back((v >> 24) & 0xff);
}

void put_name8(std::vector<uint8_t>& buf, char const* name) {
  uint8_t pad[8] = {0};
  std::size_t n = std::strlen(name);
  if (n > 8)
    n = 8;
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
  put_u16le(blob, 3);

  uint32_t const headerEnd = 2 + 3 * 16;

  put_name8(blob, "first");
  put_u32le(blob, headerEnd);
  put_u32le(blob, 4);

  put_name8(blob, "middle");
  put_u32le(blob, 0);
  put_u32le(blob, 0);

  put_name8(blob, "last");
  put_u32le(blob, headerEnd + 4);
  put_u32le(blob, 4);

  uint8_t firstData[4] = {1, 2, 3, 4};
  uint8_t lastData[4] = {5, 6, 7, 8};
  blob.insert(blob.end(), firstData, firstData + 4);
  blob.insert(blob.end(), lastData, lastData + 4);

  io::MemReader mem(blob);
  ReaderFile rf(mem);

  std::vector<SfxSample> sounds;
  REQUIRE_NOTHROW(loadSfx(sounds, rf));

  REQUIRE(sounds.size() == 3);

  REQUIRE(sounds[0].name == "first");
  REQUIRE(sounds[1].name == "middle");
  REQUIRE(sounds[2].name == "last");

  REQUIRE(sounds[0].sound != nullptr);
  REQUIRE(sounds[1].sound == nullptr);
  REQUIRE(sounds[2].sound != nullptr);
}
