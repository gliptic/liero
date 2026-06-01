#pragma once

#include <chrono>
#include <cstdint>

namespace netutil {

inline uint64_t nowMs() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace netutil
