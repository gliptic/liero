#include "bobject.hpp"

#include "gfx/color.hpp"
#include "game.hpp"
#include "constants.hpp"

void Game::createBObject(fixed x, fixed y, fixed velX, fixed velY)
{
	BObject& obj = *bobjects.newObjectReuse();
	
	obj.color = rand(common->C[NumBloodColours]) + common->C[FirstBloodColour];
	obj.x = x;
	obj.y = y;
	obj.velX = velX;
	obj.velY = velY;
}

bool BObject::process(Game& game)
{
	Common& common = *game.common;
	
	x += velX;
	y += velY;
	
	int ix = ftoi(x);
	int iy = ftoi(y);
	
	if(!game.level.inside(ix, iy))
	{
		return false;
	}
	else
	{
		PalIdx c = game.level.pixel(ix, iy);
		Material m = common.materials[c];
		
		if(m.background())
			velY += common.C[BObjGravity];
			
		if((c >= 1 && c <= 2)
		|| (c >= 77 && c <= 79)) // TODO: Read from EXE
		{
			game.level.pixel(ix, iy) = 77 + game.rand(3);
			return false;
		}
		/* This can't happen!
		else if(iy >= game.level.height)
		{
			// Nothing
		}*/
		else if(m.anyDirt())
		{
			game.level.pixel(ix, iy) = 82 + game.rand(3);
			return false;
		}
		else if(m.rock())
		{
			game.level.pixel(ix, iy) = 85 + game.rand(3);
			return false;
		}
	}
	
	return true;
}
