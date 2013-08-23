#ifndef UUID_9059AB0F9EA54E1EDF52E7BF41433D0B
#define UUID_9059AB0F9EA54E1EDF52E7BF41433D0B

#include "color.hpp"
#include "../rect.hpp"
#include "sprite.hpp"
#include <vector>

struct Level;
struct Common;
struct Rand;
struct Bitmap;

void vline(Bitmap& scr, int x, int y1, int y2, int color);

void fillRect(Bitmap& scr, int x, int y, int w, int h, int color);
void fill(Bitmap& scr, int color);
void drawBar(Bitmap& scr, int x, int y, int width, int color);
void drawRoundedBox(Bitmap& scr, int x, int y, int color, int height, int width);
void drawRoundedLineBox(Bitmap& scr, int x, int y, int color, int width, int height);
void blitImageNoKeyColour(Bitmap& scr, PalIdx* mem, int x, int y, int width, int height, int pitch);
//void blitImage(Bitmap& scr, PalIdx* mem, int x, int y, int width, int height);
void blitImage(Bitmap& scr, Sprite spr, int x, int y);
void blitImageR(Bitmap& scr, PalIdx* mem, int x, int y, int width, int height);
void blitShadowImage(Common& common, Bitmap& scr, PalIdx* mem, int x, int y, int width, int height);
void blitStone(Common& common, Level& level, bool p1, PalIdx* mem, int x, int y);
void blitFireCone(Bitmap& scr, int fc, PalIdx* mem, int x, int y);
void drawDirtEffect(Common& common, Rand& rand, Level& level, int dirtEffect, int x, int y);
void blitImageOnMap(Common& common, Level& level, PalIdx* mem, int x, int y, int width, int height);
void correctShadow(Common& common, Level& level, Rect rect);
void drawDashedLineBox(Bitmap& scr, int x, int y, int color, int color2, int num, int den, int width, int height, int phase);

void drawNinjarope(Common& common, Bitmap& scr, int fromX, int fromY, int toX, int toY);
void drawLaserSight(Bitmap& scr, Rand& rand, int fromX, int fromY, int toX, int toY);
void drawShadowLine(Common& common, Bitmap& scr, int fromX, int fromY, int toX, int toY);
void drawLine(Bitmap& scr, int fromX, int fromY, int toX, int toY, int color);

void drawGraph(Bitmap& scr,
	std::vector<double> const& data,
	int height,
	int startX,
	int startY,
	int color,
	int negColor);

void scaleDraw(
	PalIdx* src, int w, int h, std::size_t srcPitch,
	uint8_t* dest, std::size_t destPitch, int mag, uint32_t scaleFilter, uint32_t* pal32);

void preparePaletteBgra(Color realPal[256], uint32_t (&pal32)[256]);
int fitScreen(int backW, int backH, int scrW, int scrH, int& offsetX, int& offsetY, uint32_t scaleFilter);

inline void blitImageNoKeyColour(Bitmap& scr, PalIdx* mem, int x, int y, int width, int height)
{
	blitImageNoKeyColour(scr, mem, x, y, width, height, width);
}



struct Heatmap
{
	Heatmap(int width, int height, int orgWidth, int orgHeight)
	: width(width), height(height)
	, orgWidth(orgWidth), orgHeight(orgHeight)
	, map(width * height)
	{

	}

	void inc(int x, int y, int v = 1)
	{
		x = (x * width) / orgWidth;
		y = (y * height) / orgHeight;
		x = std::min(std::max(x, 0), width - 1);
		y = std::min(std::max(y, 0), height - 1);

		map[y*width + x] += v;
	}

	void incArea(int x, int y, int v = 1)
	{
		x = (x * width) / orgWidth;
		y = (y * height) / orgHeight;
		x = std::min(std::max(x, 0), width - 1);
		y = std::min(std::max(y, 0), height - 1);

		for (int y1 = -2; y1 <= 2; ++y1)
		for (int x1 = -2; x1 <= 2; ++x1)
		{
			int cx = x + x1;
			int cy = y + y1;
			if (cx >= 0 && cy >= 0 && cx < width && cy < height)
			{
				int weight = (2 * 2) * (2 * 2) - (x1 * y1) * (x1 * y1);
				map[cy*width + cx] += v * weight;
			}
		}
	}

	int width, height;
	int orgWidth, orgHeight;

	std::vector<int> map;
};

void drawHeatmap(Bitmap& scr, int x, int y, Heatmap& hm);

#endif // UUID_9059AB0F9EA54E1EDF52E7BF41433D0B
