#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

template <typename T>
struct FastObjectList {
  struct Iterator {
    Iterator(T* cur) : cur(cur) {}

    Iterator& operator++() {
      ++cur;
      return *this;
    }

    T& operator*() { return *cur; }

    T* operator->() { return cur; }

    bool operator!=(Iterator b) { return cur != b.cur; }

    T* cur;
  };

  FastObjectList(std::size_t limit = 1) : limit(limit), arr(limit) { Clear(); }

  T* GetFreeObject() {
    assert(count < limit);
    T* ptr = &arr[count++];
    return ptr;
  }

  T* NewObjectReuse() {
    T* ret;
    if (count == limit)
      ret = &arr[limit - 1];
    else
      ret = GetFreeObject();

    return ret;
  }

  T* NewObject() {
    if (count == limit) return 0;

    T* ret = GetFreeObject();
    return ret;
  }

  Iterator Begin() { return Iterator(&arr[0]); }

  Iterator End() { return Iterator(&arr[0] + count); }

  void Free(T* ptr) {
    assert(ptr < &arr[0] + count && ptr >= &arr[0]);
    *ptr = arr[--count];
  }

  void Free(Iterator i) { Free(&*i); }

  void Clear() { count = 0; }

  void Resize(std::size_t new_limit) {
    limit = new_limit;
    count = std::min(count, new_limit);
    arr.resize(new_limit);
  }

  std::size_t Size() const { return count; }

  std::size_t limit;
  std::vector<T> arr;
  std::size_t count;
};
