#ifndef UUID_9059AB0F9EA54E1EDF52E7BF41433D0B
#define UUID_9059AB0F9EA54E1EDF52E7BF41433D0B

#include <SDL/SDL.h>

#include "color.hpp"
#include "../rect.hpp"


struct Level;
struct Common;
struct Rand;

void fillRect(int x, int y, int w, int h, int color);
void drawBar(int x, int y, int width, int color);
void drawRoundedBox(int x, int y, int color, int height, int width);
void blitImageNoKeyColour(SDL_Surface* scr, PalIdx* mem, int x, int y, int width, int height, int pitch);
void blitImage(SDL_Surface* scr, PalIdx* mem, int x, int y, int width, int height);
void blitImageR(SDL_Surface* scr, PalIdx* mem, int x, int y, int width, int height);
void blitShadowImage(Common& common, SDL_Surface* scr, PalIdx* mem, int x, int y, int width, int height);
void blitStone(Common& common, Level& level, bool p1, PalIdx* mem, int x, int y);
void blitFireCone(SDL_Surface* scr, int fc, PalIdx* mem, int x, int y);
void drawDirtEffect(Common& common, Rand& rand, Level& level, int dirtEffect, int x, int y);
void blitImageOnMap(Common& common, Level& level, PalIdx* mem, int x, int y, int width, int height);
void correctShadow(Common& common, Level& level, Rect rect);

void drawNinjarope(Common& common, int fromX, int fromY, int toX, int toY);
void drawLaserSight(int fromX, int fromY, int toX, int toY);
void drawShadowLine(Common& common, int fromX, int fromY, int toX, int toY);
void drawLine(int fromX, int fromY, int toX, int toY, int color);
bool isInside(SDL_Rect const& rect, int x, int y);

inline void blitImageNoKeyColour(SDL_Surface* scr, PalIdx* mem, int x, int y, int width, int height)
{
	blitImageNoKeyColour(scr, mem, x, y, width, height, width);
}

#endif // UUID_9059AB0F9EA54E1EDF52E7BF41433D0B
