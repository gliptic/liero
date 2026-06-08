#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

struct ExactObjectListBase {
  bool used;
};

template <typename T, int Limit>
struct ExactObjectList {
  struct Range {
    Range(T* cur, T* end) : cur(cur), end(end) {}

    T* Next() {
      while (!cur->used) ++cur;

      T* ret = cur;
      ++cur;

      return ret == end ? nullptr : ret;
    }

    T* cur;
    T* end;
  };

  ExactObjectList() { Clear(); }

  T* GetFreeObject() {
    assert(std::cmp_less(count, Limit));
    ++count;

    T* ptr = nullptr;
    for (uint32_t i = 0; i < kFreeListSize; ++i) {
      if (free_list[i] != 0) {
        int const kBit = std::countr_zero(free_list[i]);
        uint32_t const kIndex = (i << 5) + kBit;
        ptr = arr + kIndex;
        free_list[i] &= ~(static_cast<uint32_t>(1) << kBit);
        break;
      }
    }

    assert(ptr && !ptr->used && ptr >= arr && ptr < arr + Limit);
    ptr->used = true;

    return ptr;
  }

  T* NewObjectReuse() {
    T* ret = nullptr;
    if (std::cmp_greater_equal(count, Limit))
      ret = &arr[Limit - 1];
    else
      ret = GetFreeObject();

    assert(ret->used && ret >= arr && ret < arr + Limit);
    return ret;
  }

  T* NewObject() {
    if (std::cmp_greater_equal(count, Limit)) return nullptr;

    T* ret = GetFreeObject();
    assert(ret->used && ret >= arr && ret < arr + Limit);
    return ret;
  }

  Range All() { return Range(&arr[0], &arr[Limit]); }

  void Free(T* ptr) {
    assert(ptr->used);
    if (ptr->used) {
      auto const kIndex = uint32_t(ptr - arr);
      free_list[kIndex >> 5] |= (static_cast<uint32_t>(1) << (kIndex & 31));

      ptr->used = false;

      assert(count > 0);
      --count;
    }
  }

  void Free(Range& r) { Free(r.cur - 1); }

  void Clear() {
    std::memset(free_list, 0xff, kFreeListSize * sizeof(uint32_t));
    count = 0;

    for (std::size_t i = 0; std::cmp_less(i, Limit); ++i) arr[i].used = false;

    arr[Limit].used = true;

    // Mark padding as used
    for (uint32_t index = Limit; index < kFreeListSize * 32; ++index)
      free_list[index >> 5] &= ~(static_cast<uint32_t>(1) << (index & 31));
  }

  std::size_t Size() const { return count; }

  T arr[Limit + 1];  // Sentinel

  static uint32_t const kFreeListSize = (Limit + 31) / 32;
  uint32_t free_list[kFreeListSize];

  std::size_t count;
};
