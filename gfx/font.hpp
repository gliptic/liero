#ifndef UUID_B06B65B783A849C7B4E509A9676180F8
#define UUID_B06B65B783A849C7B4E509A9676180F8

#include <vector>
#include <string>

#include <gvl/io/encoding.hpp>

struct Bitmap;

struct Font
{
	struct Char
	{
		unsigned char data[8*7];
		int width;
	};
	
	Font()
	: chars(250)
	{
	}
	
	void loadFromEXE();
	void drawText(Bitmap& scr, char const* str, std::size_t len, int x, int y, int color);
	int getDims(char const* str, std::size_t len, int* height = 0);
	void drawChar(Bitmap& scr, unsigned char ch, int x, int y, int color);
	
	void drawText(Bitmap& scr, std::string const& str, int x, int y, int color)
	{
		drawText(scr, str.data(), str.size(), x, y, color);
	}

	void drawText(Bitmap& scr, gvl::cell const& str, int x, int y, int color)
	{
		if (str.buffer.empty())
			return;

		if (str.text_placement != gvl::cell::left)
		{
			int w = getDims(
				reinterpret_cast<char const*>(&str.buffer[0]),
				str.buffer.size());

			if (str.text_placement == gvl::cell::center)
				x -= w / 2;
			else if (str.text_placement == gvl::cell::right)
				x -= w;
		}

		drawText(scr, reinterpret_cast<char const*>(&str.buffer[0]), str.buffer.size(), x, y, color);
	}
	
	int getDims(std::string const& str, int* height = 0)
	{
		return getDims(str.data(), str.size(), height);
	}
	
	void drawFramedText(Bitmap& scr, std::string const& text, int x, int y, int color);
	
	std::vector<Char> chars;
};

#endif // UUID_B06B65B783A849C7B4E509A9676180F8
