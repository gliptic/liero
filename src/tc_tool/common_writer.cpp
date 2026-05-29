#include "game/common_model.hpp"
#include "game/filesystem.hpp"
#include "game/io/coding.hpp"
#include "game/io/stream.hpp"

#include <fstream>
#include <sstream>

void writeSpriteTga(
	io::Writer& w,
	int imageWidth,
	int imageHeight,
	uint8_t* data,
	Palette& pal)
{
	w.put(0);
	w.put(1);
	w.put(1);

	// Palette spec
	io::write_uint16_le(w, 0);
	io::write_uint16_le(w, 256);
	w.put(24);

	io::write_uint16_le(w, 0);
	io::write_uint16_le(w, 0);
	io::write_uint16_le(w, imageWidth);
	io::write_uint16_le(w, imageHeight);
	w.put(8); // Bits per pixel
	w.put(0); // Descriptor

	for (auto const& entry : pal.entries)
	{
		w.put(entry.b << 2);
		w.put(entry.g << 2);
		w.put(entry.r << 2);
	}

	// Bottom to top
	for (std::size_t y = (std::size_t)imageHeight; y-- > 0; )
	{
		auto const* src = &data[y * imageWidth];
		w.put((uint8_t const*)src, imageWidth);
	}
}

void writeSpriteTga(
	io::Writer& w,
	SpriteSet& ss,
	Palette& pal)
{
	writeSpriteTga(w, ss.width, ss.count * ss.height, &ss.data[0], pal);
}

void commonSave(Common& common, std::string const& path)
{
	auto cfgPath = joinPath(path, "tc.cfg");
	create_directories(cfgPath);

	{
		std::ostringstream ss;
		saveTcConfig(common, ss);
		io::FileWriter textWriter(cfgPath.c_str(), "wb");
		auto str = ss.str();
		for (char c : str)
			textWriter.put(c);
	}

	for (auto& s : common.sounds)
	{
		std::string dir(joinPath(path, "sounds/"));
		create_directories(dir);

		io::FileWriter w(joinPath(dir, s.name + ".wav").c_str(), "wb");

		auto roundedSize = (s.originalData.size() + 1) & ~1;

		w.put((uint8_t const*)"RIFF", 4);
		io::write_uint32_le(w, (uint32_t)roundedSize - 8);
		w.put((uint8_t const*)"WAVE", 4);

		w.put((uint8_t const*)"fmt ", 4);
		io::write_uint32_le(w, 16); // PCM header size
		io::write_uint16_le(w, 1); // PCM
		io::write_uint16_le(w, 1); // Mono
		io::write_uint32_le(w, 22050); // Sample rate
		io::write_uint32_le(w, 22050 * 1 * 1);
		io::write_uint16_le(w, 1 * 1);
		io::write_uint16_le(w, 8);

		w.put((uint8_t const*)"data", 4);
		io::write_uint32_le(w, (uint32_t)s.originalData.size() * 1 * 1); // Data size

		auto curSize = s.originalData.size();

		for (auto& z : s.originalData)
			w.put(z + 128);

		while (curSize < roundedSize)
		{
			w.put(0); // Padding
			++curSize;
		}
	}

	{
		std::string dir(joinPath(path, "sprites/"));
		create_directories(dir);

		{
			io::FileWriter w(joinPath(dir, "small.tga").c_str(), "wb");
			writeSpriteTga(w, common.smallSprites, common.exepal);
		}

		{
			io::FileWriter w(joinPath(dir, "large.tga").c_str(), "wb");
			writeSpriteTga(w, common.largeSprites, common.exepal);
		}

		{
			io::FileWriter w(joinPath(dir, "text.tga").c_str(), "wb");
			writeSpriteTga(w, common.textSprites, common.exepal);
		}

		{
			io::FileWriter w(joinPath(dir, "font.tga").c_str(), "wb");

			std::vector<uint8_t> data(common.font.chars.size() * 7 * 8, 10);
			for (std::size_t i = 0; i < common.font.chars.size(); ++i)
			{
				Font::Char& ch = common.font.chars[i];
				uint8_t* dest = &data[i * 7 * 8];

				for (std::size_t y = 0; y < 8; ++y)
				for (std::size_t x = 0; x < ch.width; ++x)
				{
					dest[y * 7 + x] = ch.data[y * 7 + x] ? 50 : 0;
				}
			}
			writeSpriteTga(w, 7, (int)common.font.chars.size() * 8, &data[0], common.exepal);
		}
	}

	for (auto& w : common.weapons)
	{
		std::string dir(joinPath(path, "weapons/"));
		create_directories(dir);

		std::ostringstream ss;
		saveWeaponConfig(common, w, ss);
		io::FileWriter wWriter(joinPath(dir, w.idStr + ".cfg").c_str(), "wb");
		auto str = ss.str();
		for (char c : str)
			wWriter.put(c);
	}

	for (auto& w : common.nobjectTypes)
	{
		std::string dir(joinPath(path, "nobjects/"));
		create_directories(dir);

		std::ostringstream ss;
		saveNObjectConfig(common, w, ss);
		io::FileWriter nWriter(joinPath(dir, w.idStr + ".cfg").c_str(), "wb");
		auto str = ss.str();
		for (char c : str)
			nWriter.put(c);
	}

	for (auto& w : common.sobjectTypes)
	{
		std::string dir(joinPath(path, "sobjects/"));
		create_directories(dir);

		std::ostringstream ss;
		saveSObjectConfig(common, w, ss);
		io::FileWriter sWriter(joinPath(dir, w.idStr + ".cfg").c_str(), "wb");
		auto str = ss.str();
		for (char c : str)
			sWriter.put(c);
	}
}
