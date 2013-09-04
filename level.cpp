#include "level.hpp"

#include "game.hpp"
#include "gfx.hpp"
#include "gfx/color.hpp"
#include "filesystem.hpp"

#include "reader.hpp" // TODO: For lieroEXERoot. We should move that into Common.
#include <cstring>

void Level::generateDirtPattern(Common& common, Rand& rand)
{
	resize(504, 350);
	
	setPixel(0, 0, rand(7) + 12, common);
	
	for(int y = 1; y < height; ++y)
		setPixel(0, y, ((rand(7) + 12) + pixel(0, y - 1)) >> 1, common);
		
	for(int x = 1; x < width; ++x)
		setPixel(x, 0, ((rand(7) + 12) + pixel(x - 1, 0)) >> 1, common);
		
	for(int y = 1; y < height; ++y)
	for(int x = 1; x < width; ++x)
	{
		setPixel(x, y, (pixel(x - 1, y) + pixel(x, y - 1) + rand(8) + 12) / 3, common);
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
					PalIdx pix = pixel(mx, my);
					if(pix > 176 && pix < 180)
						setPixel(mx, my, (srcPix + pix) / 2, common);
					else
						setPixel(mx, my, srcPix, common);
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
		if(level.mat(x, y).rock())
			return false;
	}
	
	return true;
}

void Level::generateRandom(Common& common, Settings const& settings, Rand& rand)
{
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
		if(mat(x, y).seeShadow()
		&& mat(x + 3, y - 3).dirtRock())
		{
			setPixel(x, y, pixel(x, y) + 4, common);
		}
		
		if(pixel(x, y) >= 12
		&& pixel(x, y) <= 18
		&& mat(x + 3, y - 3).rock())
		{
			setPixel(x, y, pixel(x, y) - 2, common);
			if(pixel(x, y) < 12)
				setPixel(x, y, 12, common);
		}
	}
	
	for(int x = 0; x < width; ++x)
	{
		if(mat(x, height - 1).background())
		{
			setPixel(x, height - 1, 13, common);
		}
	}
}

void Level::resize(int width_new, int height_new)
{
	width = width_new;
	height = height_new;
	data.resize(width * height);
	materials.resize(width * height);
}

bool Level::load(Common& common, Settings const& settings, std::string const& path)
{
	resize(504, 350);

	ReaderFile f;

	try
	{
		openFileUncached(f, path);
	}
	catch (std::runtime_error&)
	{
		return false;
	}

	std::size_t len = f.len;
	bool resetPalette = true;
	
	if(len >= 504*350 + 10 + 256*3
	&& (settings.extensions && settings.loadPowerlevelPalette))
	{
		f.seekg(504*350);

		uint8_t buf[10];
		f.get(buf, 10);
		
		if(!std::memcmp("POWERLEVEL", buf, 10))
		{
			Palette pal;
			pal.read(f);
			origpal.resetPalette(pal, settings);
			
			resetPalette = false;
		}
	}
	
	f.seekg(0);
	f.get(reinterpret_cast<uint8_t*>(&data[0]), width * height);

	for (std::size_t i = 0; i < data.size(); ++i)
		materials[i] = common.materials[data[i]];

	if (resetPalette)
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
		if(!load(common, settings, settings.levelFile + ".lev"))
			generateRandom(common, settings, rand);

	}
	
	oldRandomLevel = settings.randomLevel;
	oldLevelFile = settings.levelFile;
	
	if(settings.shadow)
	{
		makeShadow(common);
	}
}

using std::vector;

inline bool free(Material m)
{
	return m.background() || m.anyDirt();
}

bool Level::selectSpawn(Rand& rand, int w, int h, gvl::ivec2& selected)
{
	vector<int> vruns(width - w + 1);
	vector<int> vdists(width - w + 1);

	Material* m = &materials[0];

	uint32_t i = 0;

	for (int y = 0; y < height; ++y)
	{
		int hrun = 0;
		int filled = 0;

		for (int x = 0; x < width; ++x)
		{
			if (free(*m))
			{
				++hrun;
			}
			else
			{
				hrun = 0;
				++filled;
			}
			++m;

			int cx = x - (w - 1);
			if (cx < 0)
				continue;

			int& vrun = vruns[cx];
			int& vdist = vdists[cx];

			if (hrun >= w)
			{
				if (vdist > 0)
				{
					vrun = 0;
					vdist = 0;
				}
				++vrun;
			}
			else
			{
				if (vrun >= h
				&& vdist <= 8
				&& filled > w / 4)
				{
					// We have a supported square at (x + 1 - w, y - h)
					++i;
					if (rand(i) < 1)
					{
						selected.x = cx;
						selected.y = y - h;
					}
				}
				++vdist;
			}

			filled -= !free(m[-w]);
		}
	}

	return i > 0;
}