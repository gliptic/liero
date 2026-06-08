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
inline T ReadN(Reader& r) {
  uint8_t buf[sizeof(T)];
  for (auto& b : buf) {
    b = r.Get();
  }
  T v;
  std::memcpy(&v, buf, sizeof(T));
  return v;
}

template <typename T, typename Writer>
inline void WriteN(Writer& w, T v) {
  uint8_t buf[sizeof(T)];
  std::memcpy(buf, &v, sizeof(T));
  for (auto b : buf) {
    w.Put(b);
  }
}

}  // namespace detail

// --- Big-endian (network byte order) readers/writers ---

template <typename Reader>
inline uint16_t ReadUint16(Reader& r) {
  auto v = detail::ReadN<uint16_t>(r);
  if constexpr (std::endian::native == std::endian::little) {
    v = std::byteswap(v);
  }
  return v;
}

template <typename Reader>
inline uint32_t ReadUint32(Reader& r) {
  auto v = detail::ReadN<uint32_t>(r);
  if constexpr (std::endian::native == std::endian::little) {
    v = std::byteswap(v);
  }
  return v;
}

template <typename Writer>
inline void WriteUint16(Writer& w, uint16_t v) {
  if constexpr (std::endian::native == std::endian::little) {
    v = std::byteswap(v);
  }
  detail::WriteN(w, v);
}

template <typename Writer>
inline void WriteUint32(Writer& w, uint32_t v) {
  if constexpr (std::endian::native == std::endian::little) {
    v = std::byteswap(v);
  }
  detail::WriteN(w, v);
}

// --- Little-endian readers/writers ---

template <typename Reader>
inline uint16_t ReadUint16Le(Reader& r) {
  auto const kV = detail::ReadN<uint16_t>(r);
  if constexpr (std::endian::native == std::endian::big) {
    kV = std::byteswap(kV);
  }
  return kV;
}

template <typename Reader>
inline uint32_t ReadUint32Le(Reader& r) {
  auto const kV = detail::ReadN<uint32_t>(r);
  if constexpr (std::endian::native == std::endian::big) {
    kV = std::byteswap(kV);
  }
  return kV;
}

template <typename Writer>
inline void WriteUint16Le(Writer& w, uint16_t v) {
  if constexpr (std::endian::native == std::endian::big) {
    v = std::byteswap(v);
  }
  detail::WriteN(w, v);
}

template <typename Writer>
inline void WriteUint32Le(Writer& w, uint32_t v) {
  if constexpr (std::endian::native == std::endian::big) {
    v = std::byteswap(v);
  }
  detail::WriteN(w, v);
}

inline int32_t Uint32AsInt32(uint32_t v) { return static_cast<int32_t>(v); }

}  // namespace io
