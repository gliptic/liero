#include "math.hpp"
#include <cmath>
#include <cstdint>

fixedvec cossin_table[128];

static uint32_t Sqr(uint32_t op) {
  uint32_t res = 0;
  uint32_t one = 1UL << 30;  // The second-to-top bit is set: use 1u << 14 for uint16_t type; use
                             // 1uL<<30 for uint32_t type

  // "one" starts at the highest power of four <= than the argument.
  while (one > op) {
    one >>= 2;
  }

  while (one != 0) {
    if (op >= res + one) {
      op -= res + one;
      res += 2 * one;
    }
    res >>= 1;
    one >>= 2;
  }
  return res;
}

int VectorLength(int x, int y) { return static_cast<int>(Sqr(x * x + y * y)); }

struct FP {
  FP(int64_t s, int bits) : s(s), bits(bits) {}

  void Reduce(int tobits) {
    int64_t const kLim = (1LL << tobits);

    while (s < (-kLim - 1) || s > kLim) {
      s >>= 1;
      --bits;
    }
  }

  int64_t Reducedfrac(int tobits) const {
    int64_t rs = s;
    int rbits = bits;
    while (rbits > 60) {
      rs >>= 1;
      --rbits;
    }

    return rs << (tobits - rbits);
  }

  int64_t s;
  int bits;
};

void PrecomputeTables() {
  int const kScalebits = 28;
  int32_t const kScale = 13176795;  // (2pi / 128) << scalebits

  for (int i = 0; i < 128; ++i) {
    int64_t rf = 0;
    int32_t c = -1;
    int32_t const kXf = i * kScale;

    // Simple Taylor series. Performance is not important.
    FP num(kXf, kScalebits);
    for (int t = 1; t < 26;) {
      rf += c * num.Reducedfrac(60);

      num.s /= ++t;
      num.Reduce(31);
      num.s = num.s * kXf;
      num.bits += kScalebits;

      num.s /= ++t;
      num.Reduce(31);
      num.s = num.s * kXf;
      num.bits += kScalebits;

      c = -c;
    }

    int const kShift = 60 - 16;

    rf += (1LL << (kShift - 1));  // Correct rounding

    auto const kR = static_cast<int32_t>(rf >> kShift);

    cossin_table[i].x = kR;
    cossin_table[(i + 32) & 0x7f].y = kR;
  }
}
