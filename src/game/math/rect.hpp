#pragma once

#include <algorithm>
#include <cmath>

template <typename T, int N>
struct BasicVec;  // unused for N != 2

template <typename T>
struct BasicVec<T, 2> {
  T x = T(0);
  T y = T(0);

  constexpr BasicVec() = default;
  constexpr BasicVec(T x, T y) : x(x), y(y) {}

  template <typename U>
  constexpr explicit BasicVec(BasicVec<U, 2> const& other)
      : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

  constexpr void Zero() {
    x = T(0);
    y = T(0);
  }

  constexpr BasicVec& operator+=(BasicVec const& r) {
    x += r.x;
    y += r.y;
    return *this;
  }
  constexpr BasicVec& operator-=(BasicVec const& r) {
    x -= r.x;
    y -= r.y;
    return *this;
  }
  constexpr BasicVec& operator*=(T s) {
    x *= s;
    y *= s;
    return *this;
  }
  constexpr BasicVec& operator/=(T s) {
    x /= s;
    y /= s;
    return *this;
  }

  friend constexpr BasicVec operator+(BasicVec a, BasicVec b) { return {a.x + b.x, a.y + b.y}; }
  friend constexpr BasicVec operator-(BasicVec a, BasicVec b) { return {a.x - b.x, a.y - b.y}; }
  friend constexpr BasicVec operator*(BasicVec a, T s) { return {a.x * s, a.y * s}; }
  friend constexpr BasicVec operator*(T s, BasicVec a) { return {a.x * s, a.y * s}; }
  friend constexpr BasicVec operator/(BasicVec a, T s) { return {a.x / s, a.y / s}; }
  friend constexpr BasicVec operator-(BasicVec a) { return {-a.x, -a.y}; }
  friend constexpr bool operator==(BasicVec a, BasicVec b) { return a.x == b.x && a.y == b.y; }
  friend constexpr bool operator!=(BasicVec a, BasicVec b) { return !(a == b); }
};

using IVec2 = BasicVec<int, 2>;
using FVec2 = BasicVec<float, 2>;

template <typename T>
struct BasicRect {
  T x1 = T(0);
  T y1 = T(0);
  T x2 = T(0);
  T y2 = T(0);

  constexpr BasicRect() = default;
  constexpr BasicRect(T x1, T y1, T x2, T y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}

  constexpr T Width() const { return x2 - x1; }
  constexpr T Height() const { return y2 - y1; }
  constexpr T CenterX() const { return (x1 + x2) / T(2); }
  constexpr T CenterY() const { return (y1 + y2) / T(2); }
  constexpr BasicVec<T, 2> Center() const { return {CenterX(), CenterY()}; }
  constexpr BasicVec<T, 2> Ul() const { return {x1, y1}; }

  constexpr bool Inside(T vx, T vy) const {
    T dx = vx - x1;
    T dy = vy - y1;
    return dx >= T(0) && dx < Width() && dy >= T(0) && dy < Height();
  }

  constexpr bool Encloses(BasicVec<T, 2> v) const { return Inside(v.x, v.y); }
  constexpr bool Encloses(T vx, T vy) const { return Inside(vx, vy); }

  constexpr bool Valid() const { return x1 <= x2 && y1 <= y2; }

  bool Intersect(BasicRect const& b) {
    x1 = std::max(x1, b.x1);
    y1 = std::max(y1, b.y1);
    x2 = std::min(x2, b.x2);
    y2 = std::min(y2, b.y2);
    return Valid();
  }

  void Join(BasicRect const& b) {
    x1 = std::min(x1, b.x1);
    y1 = std::min(y1, b.y1);
    x2 = std::max(x2, b.x2);
    y2 = std::max(y2, b.y2);
  }

  friend BasicRect operator|(BasicRect a, BasicRect const& b) {
    a.Join(b);
    return a;
  }
  friend BasicRect operator&(BasicRect a, BasicRect const& b) {
    a.Intersect(b);
    return a;
  }
};

using Rect = BasicRect<int>;
using FRect = BasicRect<float>;
