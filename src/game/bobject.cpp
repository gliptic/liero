#include "bobject.hpp"

#include "constants.hpp"
#include "game.hpp"
#include "gfx/color.hpp"

void Game::CreateBObject(fixedvec pos, fixedvec vel) {
  Common& common = *this->common;

  BObject& obj = *bobjects.NewObjectReuse();

  obj.color = rand(LC(NumBloodColours)) + LC(FirstBloodColour);
  obj.pos = pos;
  obj.vel = vel;
}

bool BObject::Process(Game& game) {
  Common& common = *game.common;

  pos += vel;

  auto ipos = Ftoi(pos);

  if (!game.level.Inside(ipos)) {
    return false;
  } else {
    PalIdx c = game.level.Pixel(ipos);
    Material m = game.level.Mat(ipos);

    if (m.Background()) vel.y += LC(BObjGravity);

    if ((c >= 1 && c <= 2) || (c >= 77 && c <= 79))  // TODO: Read from EXE
    {
      game.level.SetPixel(ipos, 77 + game.rand(3), common);
      return false;
    } else if (m.AnyDirt()) {
      game.level.SetPixel(ipos, 82 + game.rand(3), common);
      return false;
    } else if (m.Rock()) {
      game.level.SetPixel(ipos, 85 + game.rand(3), common);
      return false;
    }
  }

  return true;
}
