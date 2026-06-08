#pragma once

#include <algorithm>
#include <type_traits>
#include <vector>

using std::vector;

template <typename D, typename T>
vector<D> Convert(vector<T> const& src) {
  vector<D> v;

  v.reserve(src.size());
  for (auto& e : src) v.push_back(e);

  return std::move(v);
}

template <typename T, typename C>
vector<T> Pluck(vector<C> const& src, T(C::* a)) {
  vector<T> v;

  v.reserve(src.size());
  for (auto& e : src) v.push_back(e.*a);

  return std::move(v);
}

template <typename T>
vector<T> Stretch(vector<T> const& src, size_t len) {
  size_t const kN = src.size();
  vector<T> v(len);

  size_t i = 0;
  size_t cum = 0;
  for (size_t ci = 0; ci < len; ++ci) {
    int c = 0;
    T sum = T();

    cum += kN;

    while (i < kN) {
      sum += src[i];
      ++c;

      if (cum < len) break;

      ++i;
      cum -= len;
    }

    v[ci] = c ? sum / c : T();
  }

  return std::move(v);
}

template <typename T>
void Cumulative(vector<T>& src) {
  T prev = T();
  for (auto& v : src) prev = (v += prev);
}

template <typename T>
void Normalize(vector<T>& src, size_t limit, bool balance = true) {
  if (src.empty()) return;

  T max = *std::max_element(src.begin(), src.end());
  T min = *std::min_element(src.begin(), src.end());

  max = std::max(max, T());
  min = std::min(min, T());

  if (balance) {
    max = std::max(-min, max);
    min = std::min(-max, min);
  }

  T range = max - min;

  if (range > T()) {
    for (auto& e : src) {
      e = (e * T(limit)) / range;
    }
  }
}

template <typename T, typename Op>
vector<T> Zip(vector<T>& src, vector<T> const& other, Op op) {
  auto max = std::min(src.size(), other.size());
  vector<T> n(max);
  for (std::size_t i = 0; i < max; ++i) {
    n[i] = op(src[i], other[i]);
  }

  return std::move(n);
}
