#ifndef UUID_B06B65B783A849C7B4E509A9676180F8
#define UUID_B06B65B783A849C7B4E509A9676180F8

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
	void drawText(char const* str, std::size_t len, int x, int y, int color);
	int getDims(char const* str, std::size_t len, int* height = 0);
	void drawChar(unsigned char ch, int x, int y, int color);
	
	void drawText(std::string const& str, int x, int y, int color)
	{
		drawText(str.data(), str.size(), x, y, color);
	}
	
	int getDims(std::string const& str, int* height = 0)
	{
		return getDims(str.data(), str.size(), height);
	}
	
	void drawFramedText(std::string const& text, int x, int y, int color);
	
	std::vector<Char> chars;
};

#endif // UUID_B06B65B783A849C7B4E509A9676180F8
