#include "sprite.hpp"

#include "../reader.hpp"

#include <cassert>
#include <vector>

void SpriteSet::allocate(int width, int height, int count) {
  this->width = width;
  this->height = height;
  this->spriteSize = width * height;
  this->count = count;

  int amount = spriteSize * count;
  data.resize(amount);
}
