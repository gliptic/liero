#ifndef UUID_68BF27AE54944A5A75C91BBAD19D89F9
#define UUID_68BF27AE54944A5A75C91BBAD19D89F9

#include <string>
#include "../gfx/color.hpp"

struct Common;

struct MenuItem
{
	MenuItem(
		PalIdx color,
		PalIdx disColour,
		std::string string)
	: color(color)
	, disColour(disColour)
	, string(string)
	, hasValue(false)
	, visible(true)
	{
	}
	
	void draw(Common& common, int x, int y, bool selected, bool disabled, bool centered, int valueOffsetX);
	
	PalIdx color;
	PalIdx disColour;
	std::string string;
	
	bool hasValue;
	std::string value;
	
	bool visible;
};

#endif // UUID_68BF27AE54944A5A75C91BBAD19D89F9
