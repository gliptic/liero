#include "menuItem.hpp"

#include "../common.hpp"
#include "../gfx.hpp"

void MenuItem::draw(Common& common, int x, int y, bool selected, bool disabled, bool centered, int valueOffsetX)
{
	int wid = common.font.getDims(string);
	int valueWid = common.font.getDims(value);
	if(centered)
		x -= (wid >> 1);
	
	if(selected)
	{
		drawRoundedBox(gfx.screenBmp, x, y, 0, 7, wid);
		if(hasValue)
			drawRoundedBox(gfx.screenBmp, x + valueOffsetX - (valueWid >> 1), y, 0, 7, valueWid);
	}
	else
	{
		common.font.drawText(gfx.screenBmp, string, x + 3, y + 2, 0);
		if(hasValue)
			common.font.drawText(gfx.screenBmp, value, x + valueOffsetX - (valueWid >> 1) + 3, y + 2, 0);
	}
	
	PalIdx c;
	
	if(disabled)
		c = disColour;
	else if(selected)
		c = disabled ? 7 : 168;
	else
		c = color;
		
	common.font.drawText(gfx.screenBmp, string, x + 2, y + 1, c);
	if(hasValue)
		common.font.drawText(gfx.screenBmp, value, x + valueOffsetX - (valueWid >> 1) + 2, y + 1, c);
}

