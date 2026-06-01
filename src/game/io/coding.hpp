#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

// Big-endian and little-endian integer helpers on any type with a
// uint8_t get() / put(uint8_t) interface (io::Reader, io::Writer, ReaderFile).
//
// Uses std::byteswap (C++23) on the non-native path so the compiler can emit
// a single bswap instruction where available.

namespace io {

namespace detail {

template <typename T, typename Reader>
inline T read_n(Reader& r) {
  uint8_t buf[sizeof(T)];
  for (auto& b : buf) b = r.get();
  T v;
  std::memcpy(&v, buf, sizeof(T));
  return v;
}

template <typename T, typename Writer>
inline void write_n(Writer& w, T v) {
  uint8_t buf[sizeof(T)];
  std::memcpy(buf, &v, sizeof(T));
  for (auto b : buf) w.put(b);
}

}  // namespace detail

// --- Big-endian (network byte order) readers/writers ---

template <typename Reader>
inline uint16_t read_uint16(Reader& r) {
  uint16_t v = detail::read_n<uint16_t>(r);
  if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
  return v;
}

template <typename Reader>
inline uint32_t read_uint32(Reader& r) {
  uint32_t v = detail::read_n<uint32_t>(r);
  if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
  return v;
}

template <typename Writer>
inline void write_uint16(Writer& w, uint16_t v) {
  if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
  detail::write_n(w, v);
}

template <typename Writer>
inline void write_uint32(Writer& w, uint32_t v) {
  if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
  detail::write_n(w, v);
}

// --- Little-endian readers/writers ---

template <typename Reader>
inline uint16_t read_uint16_le(Reader& r) {
  uint16_t v = detail::read_n<uint16_t>(r);
  if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
  return v;
}

template <typename Reader>
inline uint32_t read_uint32_le(Reader& r) {
  uint32_t v = detail::read_n<uint32_t>(r);
  if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
  return v;
}

template <typename Writer>
inline void write_uint16_le(Writer& w, uint16_t v) {
  if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
  detail::write_n(w, v);
}

template <typename Writer>
inline void write_uint32_le(Writer& w, uint32_t v) {
  if constexpr (std::endian::native == std::endian::big) v = std::byteswap(v);
  detail::write_n(w, v);
}

inline int32_t uint32_as_int32(uint32_t v) { return static_cast<int32_t>(v); }

}  // namespace io
