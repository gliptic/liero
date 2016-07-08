#include "common.hpp"

#include "rand.hpp"
#include "gfx/blit.hpp"
#include "filesystem.hpp"
#include "worm.hpp"
#include <gvl/io2/convert.hpp>
#include <gvl/io2/fstream.hpp>
#include <map>
#include <string>
#include "common_model.hpp"

int Common::fireConeOffset[2][7][2] =
{
	{{-3, 1}, {-4, 0}, {-4, -2}, {-4, -4}, {-3, -5}, {-2, -6}, {0, -6}},
	{{3, 1}, {4, 0}, {4, -2}, {4, -4}, {3, -5}, {2, -6}, {0, -6}},
};

int stoneTab[3][4] =
{
	{98, 60, 61, 62},
	{63, 75, 85, 86},
	{89, 90, 97, 96}
};

char const* Texts::keyNames[177] = {
	"",
	"Esc",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
	"+",
	"`",
	"Backspace",
	"Tab",
	"Q",
	"W",
	"E",
	"R",
	"T",
	"Y",
	"U",
	"I",
	"O",
	"P",
	"\xC5",
	"^",
	"Enter",
	"Left Crtl",
	"A",
	"S",
	"D",
	"F",
	"G",
	"H",
	"J",
	"K",
	"L",
	"\xD6",
	"\xC4",
	"\xBD",
	"Left Shift",
	"'",
	"Z",
	"X",
	"C",
	"V",
	"B",
	"N",
	"M",
	",",
	".",
	"-",
	"Right Shift",
	"* (Pad)",
	"Left Alt",
	"",
	"Caps Lock",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"Num Lock",
	"Scroll Lock",
	"7 (Pad)",
	"8 (Pad)",
	"9 (Pad)",
	"- (Pad)",
	"4 (Pad)",
	"5 (Pad)",
	"6 (Pad)",
	"+ (Pad)",
	"1 (Pad)",
	"2 (Pad)",
	"3 (Pad)",
	"0 (Pad)",
	", (Pad)",
	"",
	"",
	"<",
	"F11",
	"F12",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Enter (Pad)",
	"Right Ctrl",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Print Screen",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"/ (Pad)",
	"",
	"Print Screen",
	"Right Alt",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Home",
	"Up",
	"Page Up",
	"",
	"Left",
	"",
	"Right",
	"",
	"End",
	"Down",
	"Page Down",
	"Insert",
	"Delete",
	"",
	"",
	"",
	"",
	"",
};

Texts::Texts()
{
	gameModes[0] = "Kill'em All";
	gameModes[1] = "Game of Tag";
	gameModes[2] = "Holdazone";
	gameModes[3] = "Scales of Justice";

	onoff[0] = "OFF";
	onoff[1] = "ON";

	controllers[0] = "Human";
	controllers[1] = "CPU";
	controllers[2] = "AI";

	weapStates[0] = "Menu";
	weapStates[1] = "Bonus";
	weapStates[2] = "Banned";

	copyrightBarFormat = 64;
}


void Common::drawTextSmall(Bitmap& scr, char const* str, int x, int y)
{
	for(; *str; ++str)
	{
		unsigned char c = *str - 'A';

		if(c < 26)
		{
			blitImage(scr, textSprites[c], x, y);
		}

		x += 4;
	}
}


struct OctetTextReader : gvl::octet_reader
{
	OctetTextReader(gvl::octet_reader r)
	: gvl::octet_reader(std::move(r))
	, r(*this)
	{
	}

	gvl::toml::reader<gvl::octet_reader> r;
};



#define CHECK(c) if(!(c)) goto fail

int readSpriteTga(
	gvl::octet_reader& r,
	int destImageWidth,
	int destImageHeight,
	int destCount,
	uint8_t* data,
	Palette* pal)
{
	auto idLen = r.get();
	CHECK(r.get() == 1);
	CHECK(r.get() == 1);

	// Palette spec
	CHECK(gvl::read_uint16_le(r) == 0);
	CHECK(gvl::read_uint16_le(r) == 256);
	CHECK(r.get() == 24);

	int imageWidth, imageHeight;

	CHECK(gvl::read_uint16_le(r) == 0);
	CHECK(gvl::read_uint16_le(r) == 0);

	imageWidth = gvl::read_uint16_le(r);
	imageHeight = gvl::read_uint16_le(r);
	CHECK(r.get() == 8);
	CHECK(r.get() == 0);

	r.try_skip(idLen); // Skip ID

	// TODO: Support more sprites?
	CHECK(imageWidth == destImageWidth);
	CHECK(imageHeight == destImageHeight);

	if (pal)
	{
		for (auto& entry : pal->entries)
		{
			entry.b = r.get() >> 2;
			entry.g = r.get() >> 2;
			entry.r = r.get() >> 2;
		}
	}
	else
	{
		r.try_skip(256 * 3); // Ignore palette
	}

	// Bottom to top
	for (std::size_t y = (std::size_t)imageHeight; y-- > 0; )
	{
		auto* src = &data[y * imageWidth];
		r.get((uint8_t*)src, imageWidth);
	}

	return 1;

fail:
	return 0;
}

int readSpriteTga(
	gvl::octet_reader& r,
	SpriteSet& ss,
	Palette* pal)
{
	return readSpriteTga(r, ss.width, ss.count * ss.height, ss.count, &ss.data[0], pal);
}

#undef CHECK


inline uint32_t quad(char a, char b, char c, char d)
{
	return (uint32_t)a + ((uint32_t)b << 8) + ((uint32_t)c << 16) + ((uint32_t)d << 24);
}

void Common::load(FsNode node)
{
	OctetTextReader textReader((node / "tc.cfg").toOctetReader());
	archive_text(*this, textReader.r);

	for (auto& s : sounds)
	{
		auto dir = node / "sounds";

		gvl::octet_reader r((dir / (s.name + ".wav")).toOctetReader());

		if (gvl::read_uint32_le(r) == quad('R', 'I', 'F', 'F'))
		{
			std::size_t roundedSize = gvl::read_uint32_le(r) + 8;

			(void)roundedSize; // Ignore

			if (gvl::read_uint32_le(r) == quad('W', 'A', 'V', 'E')
			 && gvl::read_uint32_le(r) == quad('f', 'm', 't', ' ')
			 && gvl::read_uint32_le(r) == 16
			 && gvl::read_uint16_le(r) == 1
			 && gvl::read_uint16_le(r) == 1
			 && gvl::read_uint32_le(r) == 22050
			 && gvl::read_uint32_le(r) == 22050 * 1 * 1
			 && gvl::read_uint16_le(r) == 1 * 1
			 && gvl::read_uint16_le(r) == 8
			 && gvl::read_uint32_le(r) == quad('d', 'a', 't', 'a'))
			{
				std::size_t dataSize = gvl::read_uint32_le(r);

				s.originalData.resize(dataSize);

				for (auto& z : s.originalData)
					z = r.get() - 128;

				s.sound = sfx_new_sound(dataSize * 2);

				s.createSound();
			}
		}
	}

	{
		auto dir = node / "sprites";

		largeSprites.allocate(16, 16, 110);
		smallSprites.allocate(7, 7, 130);
		textSprites.allocate(4, 4, 26);

		{
			gvl::octet_reader r((dir / "small.tga").toOctetReader());

			readSpriteTga(r, smallSprites, &exepal);
		}

		{
			gvl::octet_reader r((dir / "large.tga").toOctetReader());
			readSpriteTga(r, largeSprites, 0);
		}

		{
			gvl::octet_reader r((dir / "text.tga").toOctetReader());
			readSpriteTga(r, textSprites, 0);
		}

		{
			gvl::octet_reader r((dir / "font.tga").toOctetReader());

			std::vector<uint8_t> data(font.chars.size() * 7 * 8, 10);

			readSpriteTga(r, 7, (int)font.chars.size() * 8, (int)font.chars.size(), &data[0], 0);

			for (std::size_t i = 0; i < font.chars.size(); ++i)
			{
				Font::Char& ch = font.chars[i];
				uint8_t* dest = &data[i * 7 * 8];

				ch.width = 0;

				for (std::size_t y = 0; y < 8; ++y)
				for (std::size_t x = 0; x < 7; ++x)
				{
					auto p = dest[y * 7 + x];
					if (p == 0)
					{
						ch.data[y * 7 + x] = 0;
					}
					else if (p == 50)
					{
						ch.data[y * 7 + x] = 8;
					}
					else
					{
						ch.width = (int)x;
						break;
					}
				}
			}
		}
	}

	for (auto& w : weapons)
	{
		auto dir = node / "weapons";

		OctetTextReader wReader((dir / (w.idStr + ".cfg")).toOctetReader());
		archive_text(*this, w, wReader.r);
	}

	for (auto& w : nobjectTypes)
	{
		auto dir = node / "nobjects";

		OctetTextReader nReader((dir / (w.idStr + ".cfg")).toOctetReader());
		archive_text(*this, w, nReader.r);
	}

	for (auto& w : sobjectTypes)
	{
		auto dir = node / "sobjects";

		OctetTextReader sReader((dir / (w.idStr + ".cfg")).toOctetReader());
		archive_text(*this, w, sReader.r);
	}

	precompute();
}

void Common::precompute()
{
	weapOrder.resize(weapons.size());
	for (int i = 0; i < (int)weapons.size(); ++i)
	{
		weapOrder[i] = i;
		weapons[i].id = i;
	}

	std::sort(weapOrder.begin(), weapOrder.end(), [&](int a, int b) {
		return this->weapons[a].name < this->weapons[b].name;
	});

	for (int i = 0; i < (int)nobjectTypes.size(); ++i)
	{
		nobjectTypes[i].id = i;
	}

	for (int i = 0; i < (int)sobjectTypes.size(); ++i)
	{
		sobjectTypes[i].id = i;
	}

	// Precompute sprites
	wormSprites.allocate(16, 16, 2 * 2 * 21);

	for(int i = 0; i < 21; ++i)
	{
		for(int y = 0; y < 16; ++y)
		for(int x = 0; x < 16; ++x)
		{
			PalIdx pix = (largeSprites.spritePtr(16 + i) + y*16)[x];

			(wormSprite(i, 1, 0) + y*16)[x] = pix;
			if(x == 15)
				(wormSprite(i, 0, 0) + y*16)[15] = 0;
			else
				(wormSprite(i, 0, 0) + y*16)[14 - x] = pix;

			if(pix >= 30 && pix <= 34)
				pix += 9; // Change worm color

			(wormSprite(i, 1, 1) + y*16)[x] = pix;

			if(x == 15)
				(wormSprite(i, 0, 1) + y*16)[15] = 0; // A bit haxy, but works
			else
				(wormSprite(i, 0, 1) + y*16)[14 - x] = pix;
		}
	}

	fireConeSprites.allocate(16, 16, 2 * 7);

	for(int i = 0; i < 7; ++i)
	{
		for(int y = 0; y < 16; ++y)
		for(int x = 0; x < 16; ++x)
		{
			PalIdx pix = (largeSprites.spritePtr(9 + i) + y*16)[x];

			(fireConeSprite(i, 1) + y*16)[x] = pix;

			if(x == 15)
				(fireConeSprite(i, 0) + y*16)[15] = 0;
			else
				(fireConeSprite(i, 0) + y*16)[14 - x] = pix;

		}
	}
}

Common::Common()
{
}

std::string Common::guessName() const
{
	std::string const& cp = S[SCopyright2];
	auto p = cp.find('(');
	if (p == std::string::npos)
		p = cp.size();

	while (p > 0 && cp[p - 1] == ' ')
		--p;

	return cp.substr(0, p);
}

void SfxSample::createSound()
{
	int16_t* ptr = reinterpret_cast<int16_t*>(sfx_sound_data(sound));

	int prev = ((int8_t)originalData[0]) * 30;
	*ptr++ = prev;

	for(std::size_t j = 1; j < originalData.size(); ++j)
	{
		int cur = (int8_t)originalData[j] * 30;
		*ptr++ = (prev + cur) / 2;
		*ptr++ = cur;
		prev = cur;
	}

	*ptr++ = prev;
}


#if ENABLE_TRACING
void Common::ltrace(char const* category, uint32 object, char const* attribute, uint32 value)
{
	uint32 cat = *(uint32*)(category);
	uint32 attr = *(uint32*)(attribute);

	if (writeTrace)
	{
		gvl::write_uint32_le(trace_writer, cat);
		gvl::write_uint32_le(trace_writer, object);
		gvl::write_uint32_le(trace_writer, attr);
		gvl::write_uint32_le(trace_writer, value);
	}
	else
	{
		uint32 fcat = gvl::read_uint32_le(trace_reader);
		uint32 fobject = gvl::read_uint32_le(trace_reader);
		uint32 fattr = gvl::read_uint32_le(trace_reader);
		uint32 fvalue = gvl::read_uint32_le(trace_reader);

		if (fcat != cat) {
			throw std::exception();
		}
		if (fobject != object) {
			throw std::exception();
		}
		if (fattr != attr) {
			throw std::exception();
		}
		if (fvalue != value) {
			throw std::exception();
		}

	}
}
#endif
