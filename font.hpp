#ifndef LIERO_FONT_HPP
#define LIERO_FONT_HPP

#include <vector>
#include <string>

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
	void drawText(char const* str, std::size_t len, int x, int y, int colour);
	int getDims(char const* str, std::size_t len, int* height = 0);
	void drawChar(unsigned char ch, int x, int y, int colour);
	
	void drawText(std::string const& str, int x, int y, int colour)
	{
		drawText(str.data(), str.size(), x, y, colour);
	}
	
	int getDims(std::string const& str, int* height = 0)
	{
		return getDims(str.data(), str.size(), height);
	}
	
	std::vector<Char> chars;
};

#endif // LIERO_FONT_HPP
