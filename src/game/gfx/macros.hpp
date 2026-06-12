#pragma once

#define CLIP_IMAGE(clip)                         \
  {                                              \
    int const top = y - (clip).y1;               \
    if (top < 0) {                               \
      mem += -top * pitch;                       \
      height += top;                             \
      y = (clip).y1;                             \
    }                                            \
    int const bottom = y + height - ((clip).y2); \
    if (bottom > 0) height -= bottom;            \
    int const left = x - (clip).x1;              \
    if (left < 0) {                              \
      mem -= left;                               \
      width += left;                             \
      x = (clip).x1;                             \
    }                                            \
    int const right = x + width - ((clip).x2);   \
    if (right > 0) width -= right;               \
    if (width <= 0 || height <= 0) return;       \
  }
