#pragma once

#include "enumBehavior.hpp"

#include <string>

struct Common;
struct Menu;

struct ArrayEnumBehavior : EnumBehavior {
  template <int N>
  ArrayEnumBehavior(Common& common, uint32_t& v, std::string const (&arr)[N],
                    bool brokenEnter = false)
      : EnumBehavior(common, v, 0, N - 1, brokenEnter), arr(arr) {}

  void onUpdate(Menu& menu, MenuItem& item) {
    item.value = arr[v];
    item.hasValue = true;
  }

  std::string const* arr;
};
