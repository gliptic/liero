#include "sprite.hpp"

#include "../reader.hpp"

#include <cassert>
#include <vector>

void SpriteSet::Allocate(int width, int height, int count) {
  this->width = width;
  this->height = height;
  this->sprite_size = width * height;
  this->count = count;

  int amount = sprite_size * count;
  data.resize(amount);
}
