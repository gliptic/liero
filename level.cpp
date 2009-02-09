#include "level.hpp"

#include "game.hpp"
#include "gfx.hpp"
#include "colour.hpp"
#include "filesystem.hpp"

#include "reader.hpp" // TODO: For lieroEXERoot. We should move that into Common.
#include <cstring>

void Level::generateDirtPattern(Common& common, Rand& rand)
{
	width = 504;
	height = 350;
	data.resize(width * height);
	
	pixel(0, 0) = rand(7) + 12;
	
	for(int y = 1; y < height; ++y)
		pixel(0, y) = ((rand(7) + 12) + pixel(0, y - 1)) >> 1;
		
	for(int x = 1; x < width; ++x)
		pixel(x, 0) = ((rand(7) + 12) + pixel(x - 1, 0)) >> 1;
		
	for(int y = 1; y < height; ++y)
	for(int x = 1; x < width; ++x)
	{
		pixel(x, y) = (pixel(x - 1, y) + pixel(x, y - 1) + rand(8) + 12) / 3;
	}
	
	// TODO: Optimize the following
	
	int count = rand(100);
	
	for(int i = 0; i < count; ++i)
	{
		int x = rand(width) - 8;
		int y = rand(height) - 8;
		
		int temp = rand(4) + 69;
		
		PalIdx* image = common.largeSprites.spritePtr(temp);
		
		for(int cy = 0; cy < 16; ++cy)
		{
			int my = cy + y;
			if(my >= height)
				break;
				
			if(my < 0)
				continue;
			
			for(int cx = 0; cx < 16; ++cx)
			{
				int mx = cx + x;
				if(mx >= width)
					break;
					
				if(mx < 0)
					continue;
					
				PalIdx srcPix = image[(cy << 4) + cx];
				if(srcPix > 0)
				{
					PalIdx& pix = pixel(mx, my);
					if(pix > 176 && pix < 180)
						pix = (srcPix + pix) / 2;
					else
						pix = srcPix;
				}
			}
		}
	}
	
	count = rand(15);
	
	for(int i = 0; i < count; ++i)
	{
		int x = rand(width) - 8;
		int y = rand(height) - 8;
		
		int which = rand(4) + 56;
		
		blitStone(common, *this, false, common.largeSprites.spritePtr(which), x, y);
	}
}

bool isNoRock(Common& common, Level& level, int size, int x, int y)
{
	Rect rect(x, y, x + size + 1, y + size + 1);
	
	rect.intersect(Rect(0, 0, level.width, level.height));
	
	for(int y = rect.y1; y < rect.y2; ++y)
	for(int x = rect.x1; x < rect.x2; ++x)
	{
		if(common.materials[level.pixel(x, y)].rock())
			return false;
	}
	
	return true;
}

void Level::generateRandom(Common& common, Settings const& settings, Rand& rand)
{
	// TODO: Skipping this is a minor deviation of liero behaviour
	/*
	gfx.settings.levelFile.clear();
	gfx.settings.randomLevel = true;*/
	//gfx.resetPalette(common.exepal); // TODO: Palette should be in Game (or Level?) and Game should transfer it to Gfx when needed
	origpal.resetPalette(common.exepal, settings);
	
	generateDirtPattern(common, rand);
	
	int count = rand(50) + 5;
	
	for(int i = 0; i < count; ++i)
	{
		int cx = rand(width) - 8;
		int cy = rand(height) - 8;
		
		int dx = rand(11) - 5;
		int dy = rand(5) - 2;
		
		int count2 = rand(12);
		
		for(int j = 0; j < count2; ++j)
		{
			int count3 = rand(5);
		
			for(int k = 0; k < count3; ++k)
			{
				cx += dx;
				cy += dy;
				drawDirtEffect(common, rand, *this, 1, cx, cy); // TODO: Check if it really should be dirt effect 1
			}
			
			cx -= (count3 + 1) * dx; // TODO: Check if it really should be (count3 + 1)
			cy -= (count3 + 1) * dy; // TODO: Check if it really should be (count3 + 1)
			
			cx += rand(7) - 3;
			cy += rand(15) - 7;
		}
	}
	
	count = rand(15) + 5;
	for(int i = 0; i < count; ++i)
	{
		int cx, cy;
		do
		{
			cx = rand(width) - 16;
			
			if(rand(4) == 0)
				cy = height - 1 - rand(20);
			else
				cy = rand(height) - 16;
		}
		while(!isNoRock(common, *this, 32, cx, cy));
		
		int rock = rand(3);
		
		blitStone(common, *this, false, common.largeSprites.spritePtr(stoneTab[rock][0]), cx, cy);
		blitStone(common, *this, false, common.largeSprites.spritePtr(stoneTab[rock][1]), cx + 16, cy);
		blitStone(common, *this, false, common.largeSprites.spritePtr(stoneTab[rock][2]), cx, cy + 16);
		blitStone(common, *this, false, common.largeSprites.spritePtr(stoneTab[rock][3]), cx + 16, cy + 16);
	}
	
	count = rand(25) + 5;
	
	for(int i = 0; i < count; ++i)
	{
		int cx, cy;
		do
		{
			cx = rand(width) - 8;
			
			if(rand(5) == 0)
				cy = height - 1 - rand(13);
			else
				cy = rand(height) - 8;
		}
		while(!isNoRock(common, *this, 15, cx, cy));
		
		blitStone(common, *this, false, common.largeSprites.spritePtr(rand(6) + 3), cx, cy);
	}
}

void Level::makeShadow(Common& common)
{
	for(int x = 0; x < width - 3; ++x)
	for(int y = 3; y < height; ++y)
	{
		if(common.materials[pixel(x, y)].seeShadow()
		&& common.materials[pixel(x + 3, y - 3)].dirtRock())
		{
			pixel(x, y) += 4;
		}
		
		if(pixel(x, y) >= 12
		&& pixel(x, y) <= 18
		&& common.materials[pixel(x + 3, y - 3)].rock())
		{
			pixel(x, y) -= 2;
			if(pixel(x, y) < 12)
				pixel(x, y) = 12;
		}
	}
	
	for(int x = 0; x < width; ++x)
	{
		if(common.materials[pixel(x, height - 1)].background())
		{
			pixel(x, height - 1) = 13;
		}
	}
}

bool Level::load(Common& common, Settings const& settings, std::string const& path)
{
	width = 504;
	height = 350;
	data.resize(width * height);
	
	ScopedFile f(tolerantFOpen(path.c_str(), "rb"));
	if(!f)
		return false;
		
	std::size_t len = fileLength(f);
	
	if(len > 504*350
	&& common.loadPowerlevelPalette)
	{
		std::fseek(f, 504*350, SEEK_SET);
		char buf[10];
		
		std::fread(buf, 1, 10, f);
		
		if(!std::memcmp("POWERLEVEL", buf, 10))
		{
			Palette pal;
			pal.read(f);
			origpal.resetPalette(pal, settings);
			
			std::fseek(f, 0, SEEK_SET);
			std::fread(&data[0], 1, width * height, f);
			return true;
		}
	}
	
	std::fseek(f, 0, SEEK_SET);
	std::fread(&data[0], 1, width * height, f);
	origpal.resetPalette(common.exepal, settings);
	
	return true;
}

void Level::generateFromSettings(Common& common, Settings const& settings, Rand& rand)
{
	if(settings.randomLevel)
	{
		generateRandom(common, settings, rand);
	}
	else
	{
		// TODO: Check .LEV as well as .lev
		if(!load(common, settings, joinPath(lieroEXERoot, settings.levelFile + ".lev")))
			generateRandom(common, settings, rand);

	}
	
	oldRandomLevel = settings.randomLevel;
	oldLevelFile = settings.levelFile;
	
	if(settings.shadow)
	{
		makeShadow(common);
	}
}
