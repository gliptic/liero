#pragma once

#include "enumBehavior.hpp"
#include "menuItem.hpp"

#include <string>

struct Common;
struct Menu;

struct ArrayEnumBehavior : EnumBehavior {
  template <int N>
  ArrayEnumBehavior(Common& common, uint32_t& v, std::string const (&arr)[N],
                    bool broken_enter = false)
      : EnumBehavior(common, v, 0, N - 1, broken_enter), arr(arr) {}

  void OnUpdate(Menu& /*menu*/, MenuItem& item) override {
    item.value = arr[v];
    item.has_value = true;
  }

  std::string const* arr;
};
