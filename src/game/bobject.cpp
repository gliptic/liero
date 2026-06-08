#include "bobject.hpp"

#include "constants.hpp"
#include "game.hpp"
#include "gfx/color.hpp"

void Game::CreateBObject(fixedvec pos, fixedvec vel) {
  Common const& common = *this->common;

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
  }
  PalIdx const kC = game.level.Pixel(ipos);
  Material const kM = game.level.Mat(ipos);

  if (kM.Background()) {
    vel.y += LC(BObjGravity);
  }

  if ((kC >= 1 && kC <= 2) || (kC >= 77 && kC <= 79))  // TODO: Read from EXE
  {
    game.level.SetPixel(ipos, 77 + game.rand(3), common);
    return false;
  }
  if (kM.AnyDirt()) {
    game.level.SetPixel(ipos, 82 + game.rand(3), common);
    return false;
  }
  if (kM.Rock()) {
    game.level.SetPixel(ipos, 85 + game.rand(3), common);
    return false;
  }

  return true;
}
