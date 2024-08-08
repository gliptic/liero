#include "bonus.hpp"
#include "game.hpp"
#include "constants.hpp"
#include <cstdlib>

void Bonus::process(Game& game)
{
	Common& common = *game.common;

	y += velY;

	int ix = ftoi(x), iy = ftoi(y);

	assert(ix >= 0 && ix < game.level.width);

	if(game.level.inside(ix, iy + 1)
	&& game.level.mat(ix, iy + 1).background())
	{
		velY += LC(BonusGravity);
	}

	int inewY = ftoi(y + velY);
	if(inewY < 0 || inewY >= game.level.height - 1
	|| game.level.mat(ix, inewY).dirtRock())
	{
		velY = -(velY * LC(BonusBounceMul)) / LC(BonusBounceDiv);

		if(std::abs(velY) < 100) // TODO: Read from EXE
			velY = 0;
	}

	if(--timer <= 0)
	{
		common.sobjectTypes[common.bonusSObjects[frame]].create(game, ix, iy, 0, 0);
		if(used)
			game.bonuses.free(this);
	}
}
