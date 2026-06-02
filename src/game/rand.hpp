#pragma once

#include <cassert>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>

// Deterministic RNG wrapper around std::mt19937. State is ~2.5 KB; serialize()
// / deserialize() use the standardised text stream format that the C++
// standard guarantees is portable across implementations — required for
// network sync and replay reproducibility.
struct Rand {
  std::mt19937 engine{0x1337u};
  uint32_t last = 0;

  Rand() = default;
  explicit Rand(uint32_t s) : engine(s) {}

  void Seed(uint32_t s) {
    engine.seed(s);
    last = 0;
  }

  uint32_t operator()() { return last = engine(); }

  // Number in [0, max). Uses Lemire's multiply-shift bound — portable across
  // stdlibs (std::uniform_int_distribution is implementation-defined).
  uint32_t operator()(uint32_t max) {
    return static_cast<uint32_t>((static_cast<uint64_t>((*this)()) * max) >> 32);
  }

  // Number in [min, max).
  uint32_t operator()(uint32_t min, uint32_t max) {
    assert(min <= max);
    return operator()(max - min) + min;
  }

  // Number in [0.0, 1.0).
  double GetDouble() { return (*this)() / 4294967296.0; }

  double GetDouble(double max) { return GetDouble() * max; }

  std::string serialize() const {
    std::ostringstream oss;
    oss << engine;
    return oss.str();
  }

  void Deserialize(std::string const& s) {
    std::istringstream iss(s);
    iss >> engine;
  }

  bool operator==(Rand const& other) const {
    return last == other.last && serialize() == other.serialize();
  }
  bool operator!=(Rand const& other) const { return !(*this == other); }
};

template <typename Ar>
void ArchiveRand(Ar ar, Rand& r) {
  if constexpr (Ar::in) {
    uint32_t len = 0;
    ar.ui32(len);
    std::string s(len, '\0');
    for (uint32_t i = 0; i < len; ++i) {
      uint8_t b = 0;
      ar.ui8(b);
      s[i] = static_cast<char>(b);
    }
    r.Deserialize(s);
    ar.ui32(r.last);
  } else {
    std::string s = r.serialize();
    uint32_t len = static_cast<uint32_t>(s.size());
    ar.ui32(len);
    for (uint32_t i = 0; i < len; ++i) {
      uint8_t b = static_cast<uint8_t>(s[i]);
      ar.ui8(b);
    }
    ar.ui32(r.last);
  }
}
