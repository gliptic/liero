#include "palette.hpp"

#include "../settings.hpp"
#include "../reader.hpp"
#include "../gfx.hpp"

void Palette::activate(Color realPal[256])
{
	for(int i = 0; i < 256; ++i)
	{
		realPal[i].r = entries[i].r << 2;
		realPal[i].g = entries[i].g << 2;
		realPal[i].b = entries[i].b << 2;
	}
}

int fadeValue(int v, int amount)
{
	assert(v < 64);
	v = (v * amount) >> 5;
	if(v < 0) v = 0;
	return v;
}

int lightUpValue(int v, int amount)
{
	v = (v * (32 - amount) + amount*63) >> 5;
	if(v > 63) v = 63;
	return v;
}

void Palette::fade(int amount)
{
	if(amount >= 32)
		return;

	for(int i = 0; i < 256; ++i)
	{
		entries[i].r = fadeValue(entries[i].r, amount);
		entries[i].g = fadeValue(entries[i].g, amount);
		entries[i].b = fadeValue(entries[i].b, amount);
	}
}

void Palette::lightUp(int amount)
{
	for(int i = 0; i < 256; ++i)
	{
		entries[i].r = lightUpValue(entries[i].r, amount);
		entries[i].g = lightUpValue(entries[i].g, amount);
		entries[i].b = lightUpValue(entries[i].b, amount);
	}
}

void Palette::rotateFrom(Palette& source, int from, int to, unsigned dist)
{
	int count = (to - from + 1);
	dist %= count;

	for(int i = 0; i < count; ++i)
	{
		entries[from + i] = source.entries[from + ((i + count - dist) % count)];
	}
}

void Palette::clear()
{
	std::memset(entries, 0, sizeof(entries));
}

void Palette::read(gvl::octet_reader& r)
{
	for(int i = 0; i < 256; ++i)
	{
		uint8_t rgb[3];
		r.get(rgb, 3);

		entries[i].r = rgb[0] & 63;
		entries[i].g = rgb[1] & 63;
		entries[i].b = rgb[2] & 63;
	}
}

int const Palette::wormColourIndexes[2] = {0x58, 0x78}; // TODO: Read from EXE?

void Palette::setWormColour(int i, WormSettings const& settings)
{
	int idx = settings.color;

	setWormColoursSpan(idx, settings.rgb);

	for(int j = 0; j < 6; ++j)
	{
		entries[wormColourIndexes[i] + j] = entries[idx + (j % 3) - 1];
	}

	for(int j = 0; j < 3; ++j)
	{
		entries[129 + i * 4 + j] = entries[idx + j];
	}
}


void Palette::setWormColours(Settings const& settings)
{
	for(int i = 0; i < 2; ++i)
	{
		setWormColour(i, *settings.wormSettings[i]);
	}
}
