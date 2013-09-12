#include "bobject.hpp"

#include "gfx/color.hpp"
#include "game.hpp"
#include "constants.hpp"

void Game::createBObject(fixed x, fixed y, fixed velX, fixed velY)
{
	Common& common = *this->common;

	BObject& obj = *bobjects.newObjectReuse();
	
	obj.color = rand(LC(NumBloodColours)) + LC(FirstBloodColour);
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
		//Material m = common.materials[c];
		Material m = game.level.mat(ix, iy);
		
		if(m.background())
			velY += LC(BObjGravity);
			
		if((c >= 1 && c <= 2)
		|| (c >= 77 && c <= 79)) // TODO: Read from EXE
		{
			game.level.setPixel(ix, iy, 77 + game.rand(3), common);
			return false;
		}
		else if(m.anyDirt())
		{
			game.level.setPixel(ix, iy, 82 + game.rand(3), common);
			return false;
		}
		else if(m.rock())
		{
			game.level.setPixel(ix, iy, 85 + game.rand(3), common);
			return false;
		}
	}
	
	return true;
}
