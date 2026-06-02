#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// CP437 ↔ Unicode conversion.
//
// The Liero bitmap font is byte-indexed CP437: each byte selects one glyph,
// and bytes <0x20 are reserved for in-band control codes (0 = newline, 1 and
// 252+ are skipped). tctool reads strings from the original DOS executable as
// raw CP437 bytes; tc.cfg, however, is TOML and stores strings as UTF-8.
//
// These helpers bridge the two: tctool calls cp437BytesToUtf8() when writing
// strings to tc.cfg, and the font calls utf8DecodeNext()/unicodeToCp437() to
// turn UTF-8 codepoints back into glyph indices at draw time. Bytes 0x00–0x1F
// pass through untouched on both sides so Liero's control codes survive.

namespace cp437 {

// Map a CP437 byte (0x00–0xFF) to its Unicode codepoint. Bytes 0x00–0x1F map
// to themselves (control codes used in-band by the font); 0x20–0x7E are ASCII;
// 0x7F maps to U+007F; 0x80–0xFF use the standard CP437 → Unicode table.
char32_t ByteToUnicode(uint8_t b);

// Reverse mapping. Returns -1 if cp has no CP437 representation.
int UnicodeToByte(char32_t cp);

// Decode one UTF-8 codepoint at str[i], advance i past it, and return the
// codepoint. On malformed input, returns U+FFFD and advances by one byte.
char32_t Utf8DecodeNext(char const* str, std::size_t len, std::size_t& i);

// Convert a string of CP437 bytes into UTF-8.
std::string Cp437BytesToUtf8(std::string_view in);

}  // namespace cp437
