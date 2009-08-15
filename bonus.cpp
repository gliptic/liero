#include "bonus.hpp"
#include "game.hpp"
#include "constants.hpp"
#include <iostream>
#include <cstdlib>

void Bonus::process(Game& game)
{
	Common& common = *game.common;
	
	y += velY;
	
	int ix = ftoi(x), iy = ftoi(y);
	
	if(game.level.inside(ix, iy + 1)
	&& common.materials[game.level.pixel(ix, iy + 1)].background())
	{
		velY += common.C[BonusGravity];
	}
		
	int inewY = ftoi(y + velY);
	if(inewY < 0 || inewY >= game.level.height - 1
	|| common.materials[game.level.pixel(ix, inewY)].dirtRock())
	{
		velY = -(velY * common.C[BonusBounceMul]) / common.C[BonusBounceDiv];
		
		if(std::abs(velY) < 100) // TODO: Read from EXE
			velY = 0;
	}
	
	if(--timer <= 0)
	{
		common.sobjectTypes[common.bonusSObjects[frame]].create(game, ix, iy, 0);
		if(used)
			game.bonuses.free(this);
	}
}
