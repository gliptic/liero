#ifndef LIERO_LEVEL_HPP
#define LIERO_LEVEL_HPP

#include <vector>
#include <string>
#include <cstdio>
#include <utility>
#include "rect.hpp"
#include "palette.hpp"
#include <SDL/SDL.h>

struct Game;
struct Settings;
struct Rand;
struct Common;

struct Level
{
	Level()
	: width(0)
	, height(0)
	{
	}
	
	bool load(Common& common, Settings const& settings, std::string const& path);
	
	void generateDirtPattern(Common& common, Rand& rand);
	void generateRandom(Common& common, Settings const& settings, Rand& rand);
	void makeShadow(Common& common);
	void generateFromSettings(Common& common, Settings const& settings, Rand& rand);
	
	unsigned char& pixel(int x, int y)
	{
		return data[x + y*width];
	}
	
	unsigned char checkedPixelWrap(int x, int y)
	{
		unsigned int idx = static_cast<unsigned int>(x + y*width);
		if(idx < data.size())
			return data[idx];
		return 0;
	}
	
	bool inside(int x, int y)
	{
		return static_cast<unsigned int>(x) < static_cast<unsigned int>(width)
		    && static_cast<unsigned int>(y) < static_cast<unsigned int>(height);
	}
	
	void swap(Level& other)
	{
		data.swap(other.data);
		std::swap(width, other.width);
		std::swap(height, other.height);
		std::swap(origpal, other.origpal);
		std::swap(oldRandomLevel, other.oldRandomLevel);
		std::swap(oldLevelFile, other.oldLevelFile);
	}
	
	Rect rect()
	{
		return Rect(0, 0, width, height);
	}
	
	void resize(int width_new, int height_new);
	
	std::vector<unsigned char> data;
	
	bool oldRandomLevel;
	std::string oldLevelFile;
	int width;
	int height;
	Palette origpal;
};

#endif // LIERO_LEVEL_HPP
