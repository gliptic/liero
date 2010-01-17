#ifndef LIERO_MENU_MENU_ITEM_HPP
#define LIERO_MENU_MENU_ITEM_HPP

#include <string>
#include "../colour.hpp"

struct Common;

struct MenuItem
{
	MenuItem(
		PalIdx colour,
		PalIdx disColour,
		std::string string)
	: colour(colour)
	, disColour(disColour)
	, string(string)
	, hasValue(false)
	, visible(true)
	{
	}
	
	void draw(Common& common, int x, int y, bool selected, bool disabled, bool centered, int valueOffsetX);
	
	PalIdx colour;
	PalIdx disColour;
	std::string string;
	
	bool hasValue;
	std::string value;
	
	bool visible;
};

#endif // LIERO_MENU_MENU_ITEM_HPP
