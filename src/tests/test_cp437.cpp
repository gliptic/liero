#include <catch2/catch_test_macros.hpp>

#include "cp437.hpp"

TEST_CASE("cp437: ASCII round-trips unchanged") {
  for (uint8_t b = 0; b < 0x80; ++b) {
    REQUIRE(cp437::ByteToUnicode(b) == b);
    REQUIRE(cp437::UnicodeToByte(b) == b);
  }
}

TEST_CASE("cp437: known high-half mappings") {
  // The bug that motivated this module: CP437 0x84 is 'ä' (U+00E4),
  // not the U+0084 control character that the legacy tc.cfg stored.
  REQUIRE(cp437::ByteToUnicode(0x84) == U'ä');
  REQUIRE(cp437::UnicodeToByte(U'ä') == 0x84);

  REQUIRE(cp437::ByteToUnicode(0x8F) == U'Å');
  REQUIRE(cp437::UnicodeToByte(U'Å') == 0x8F);
  REQUIRE(cp437::ByteToUnicode(0x99) == U'Ö');
  REQUIRE(cp437::ByteToUnicode(0xAB) == U'½');
}

TEST_CASE("cp437: unmapped codepoint returns -1") {
  REQUIRE(cp437::UnicodeToByte(0x1F600) == -1);  // emoji
  REQUIRE(cp437::UnicodeToByte(0x0084) == -1);   // C1 control char
}

TEST_CASE("cp437: bytes -> UTF-8 -> codepoint round-trip") {
  std::string raw;
  for (int b = 0; b < 256; ++b) raw.push_back(static_cast<char>(b));

  std::string utf8 = cp437::Cp437BytesToUtf8(raw);

  std::size_t i = 0;
  for (int b = 0; b < 256; ++b) {
    char32_t cp = cp437::Utf8DecodeNext(utf8.data(), utf8.size(), i);
    REQUIRE(cp == cp437::ByteToUnicode(static_cast<uint8_t>(b)));
  }
  REQUIRE(i == utf8.size());
}

TEST_CASE("cp437: malformed UTF-8 yields replacement char") {
  // Lone continuation byte
  char buf[] = {'\x80'};
  std::size_t i = 0;
  REQUIRE(cp437::Utf8DecodeNext(buf, 1, i) == 0xFFFD);
  REQUIRE(i == 1);

  // Truncated 2-byte sequence
  char buf2[] = {'\xC3'};
  i = 0;
  REQUIRE(cp437::Utf8DecodeNext(buf2, 1, i) == 0xFFFD);
}
