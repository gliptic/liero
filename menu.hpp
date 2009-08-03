#ifndef LIERO_MENU_HPP
#define LIERO_MENU_HPP

#include <cstddef>
#include <string>
#include <cstdio>
#include <vector>
#include "colour.hpp"

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
	{
	}
	
	void draw(Common& common, int x, int y, bool selected, bool disabled, bool centered);
	
	PalIdx colour;
	PalIdx disColour;
	std::string string;
};

struct Menu
{
	void readItems(FILE* f, int length, int count, bool colourPrefix, PalIdx colour = 0, PalIdx disColour = 0);
	
	void readItem(FILE* f, int offset, PalIdx colour = 0, PalIdx disColour = 0);
	
	Menu(bool centered = false)
	: itemHeight(8)
	, centered(centered)
	, selection(0)
	{
	}
	
	void draw(Common& common, int x, int y, bool disabled/*, int selection = -1*/, int firstItem = 0, int lastItem = -1);
	
	std::vector<MenuItem> items;
	int itemHeight;
	bool centered;
	int selection;
};

#endif // LIERO_MENU_HPP
