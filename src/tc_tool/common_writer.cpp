#include "game/common_model.hpp"
#include <gvl/io2/convert.hpp>
#include <gvl/io2/fstream.hpp>
#include <gvl/serialization/coding.hpp>
#include <gvl/serialization/toml.hpp>

#include "game/filesystem.hpp"

struct OctetTextWriter : gvl::octet_writer
{
	OctetTextWriter(std::string const& path)
	: gvl::octet_writer(gvl::sink(new gvl::file_bucket_pipe(path.c_str(), "wb")))
	, w(*this)
	{
	}

	gvl::toml::writer<gvl::octet_writer> w;
};

void writeSpriteTga(
	gvl::octet_writer& w,
	int imageWidth,
	int imageHeight,
	uint8_t* data,
	Palette& pal)
{
	w.put(0);
	w.put(1);
	w.put(1);

	// Palette spec
	gvl::write_uint16_le(w, 0);
	gvl::write_uint16_le(w, 256);
	w.put(24);

	gvl::write_uint16_le(w, 0);
	gvl::write_uint16_le(w, 0);
	gvl::write_uint16_le(w, imageWidth);
	gvl::write_uint16_le(w, imageHeight);
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
	gvl::octet_writer& w,
	SpriteSet& ss,
	Palette& pal)
{
	writeSpriteTga(w, ss.width, ss.count * ss.height, &ss.data[0], pal);
}

void commonSave(Common& common, std::string const& path)
{
	auto cfgPath = joinPath(path, "tc.cfg");
	create_directories(cfgPath);
	OctetTextWriter textWriter(cfgPath);
	archive_text(common, textWriter.w);

	for (auto& s : common.sounds)
	{
		std::string dir(joinPath(path, "sounds/"));
		create_directories(dir);

		gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, s.name + ".wav").c_str(), "wb")));

		auto roundedSize = (s.originalData.size() + 1) & ~1;

		w.put((uint8_t const*)"RIFF", 4);
		gvl::write_uint32_le(w, (uint32_t)roundedSize - 8);
		w.put((uint8_t const*)"WAVE", 4);

		w.put((uint8_t const*)"fmt ", 4);
		gvl::write_uint32_le(w, 16); // PCM header size
		gvl::write_uint16_le(w, 1); // PCM
		gvl::write_uint16_le(w, 1); // Mono
		gvl::write_uint32_le(w, 22050); // Sample rate
		gvl::write_uint32_le(w, 22050 * 1 * 1);
		gvl::write_uint16_le(w, 1 * 1);
		gvl::write_uint16_le(w, 8);

		w.put((uint8_t const*)"data", 4);
		gvl::write_uint32_le(w, (uint32_t)s.originalData.size() * 1 * 1); // Data size

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
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "small.tga").c_str(), "wb")));
			writeSpriteTga(w, common.smallSprites, common.exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "large.tga").c_str(), "wb")));
			writeSpriteTga(w, common.largeSprites, common.exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "text.tga").c_str(), "wb")));
			writeSpriteTga(w, common.textSprites, common.exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "font.tga").c_str(), "wb")));

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

		OctetTextWriter wWriter(joinPath(dir, w.idStr + ".cfg"));
		archive_text(common, w, wWriter.w);
	}

	for (auto& w : common.nobjectTypes)
	{
		std::string dir(joinPath(path, "nobjects/"));
		create_directories(dir);

		OctetTextWriter nWriter(joinPath(dir, w.idStr + ".cfg"));
		archive_text(common, w, nWriter.w);
	}

	for (auto& w : common.sobjectTypes)
	{
		std::string dir(joinPath(path, "sobjects/"));
		create_directories(dir);

		OctetTextWriter sWriter(joinPath(dir, w.idStr + ".cfg"));
		archive_text(common, w, sWriter.w);
	}
}
