#ifndef LIERO_LEVEL_HPP
#define LIERO_LEVEL_HPP

#include <vector>
#include <string>
#include <cstdio>
#include <utility>
#include "rect.hpp"
#include "gfx/palette.hpp"
#include "material.hpp"
#include "common.hpp"
#include <gvl/math/vec.hpp>

struct Game;
struct Settings;
struct Rand;
struct Common;

struct Level
{
	Level(Common& common)
	: width(0)
	, height(0)
	{
		zeroMaterial = common.materials[0];
	}
	
	bool load(Common& common, Settings const& settings, std::string const& path);
	
	void generateDirtPattern(Common& common, Rand& rand);
	void generateRandom(Common& common, Settings const& settings, Rand& rand);
	void makeShadow(Common& common);
	void generateFromSettings(Common& common, Settings const& settings, Rand& rand);
	bool selectSpawn(Rand& rand, int w, int h, gvl::ivec2& selected);
		
	unsigned char pixel(int x, int y)
	{
		return data[x + y*width];
	}

	unsigned char* pixelp(int x, int y)
	{
		return &data[x + y*width];
	}

	void setPixel(int x, int y, PalIdx w, Common& common)
	{
		data[x + y*width] = w;
		materials[x + y*width] = common.materials[w];
	}

	Material& mat(int x, int y)
	{
		return materials[x + y*width];
	}

	Material* matp(int x, int y)
	{
		return &materials[x + y*width];
	}
	
	unsigned char checkedPixelWrap(int x, int y)
	{
		unsigned int idx = static_cast<unsigned int>(x + y*width);
		if(idx < data.size())
			return data[idx];
		return 0;
	}

	Material checkedMatWrap(int x, int y)
	{
		unsigned int idx = static_cast<unsigned int>(x + y*width);
		if(idx < materials.size())
			return materials[idx];
		return zeroMaterial;
	}
	
	bool inside(int x, int y)
	{
		return static_cast<unsigned int>(x) < static_cast<unsigned int>(width)
		    && static_cast<unsigned int>(y) < static_cast<unsigned int>(height);
	}
	
	void swap(Level& other)
	{
		data.swap(other.data);
		materials.swap(other.materials);
		std::swap(width, other.width);
		std::swap(height, other.height);
		std::swap(origpal, other.origpal);
		std::swap(oldRandomLevel, other.oldRandomLevel);
		std::swap(oldLevelFile, other.oldLevelFile);
		std::swap(zeroMaterial, other.zeroMaterial);
	}
	
	Rect rect()
	{
		return Rect(0, 0, width, height);
	}
	
	void resize(int width_new, int height_new);
	
	std::vector<unsigned char> data;
	std::vector<Material> materials;
	
	bool oldRandomLevel;
	std::string oldLevelFile;
	int width, height;
	Palette origpal;
	Material zeroMaterial;
};

#endif // LIERO_LEVEL_HPP
