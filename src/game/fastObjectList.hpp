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
    T* ret = nullptr;
    if (count == limit)
      ret = &arr[limit - 1];
    else
      ret = GetFreeObject();

    return ret;
  }

  T* NewObject() {
    if (count == limit) return nullptr;

    T* ret = GetFreeObject();
    return ret;
  }

  Iterator Begin() { return Iterator(arr.data()); }

  Iterator End() { return Iterator(arr.data() + count); }

  void Free(T* ptr) {
    assert(ptr < arr.data() + count && ptr >= arr.data());
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
