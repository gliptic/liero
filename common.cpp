#include "common.hpp"

#include "reader.hpp"
#include "rand.hpp"
#include "gfx/blit.hpp"
#include "filesystem.hpp"
#include "worm.hpp"
#include <gvl/io2/convert.hpp>
#include <gvl/io2/fstream.hpp>
#include <map>
#include <string>

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

#include <gvl/serialization/toml.hpp>

template<typename T>
struct ObjectResolver
{
	ObjectResolver(Common& common, vector<T>& vec)
	: common(common)
	, vec(vec)
	{
	}

	void r2v(int& v)
	{
		v = -1;
	}

	void r2v(int& v, std::string const& str)
	{
		for (std::size_t i = 0; vec.size(); ++i)
		{
			auto& n = vec[i];
			if (n.idStr == str)
			{
				v = (int)i;
				return;
			}
		}
		v = 0;
	}

	template<typename Archive>
	void v2r(Archive& ar, int v)
	{
		if (v < 0)
			ar.null(0);
		else
			ar.str(0, vec[v].idStr);
	}

	Common& common;
	vector<T>& vec;
};

template<typename Archive>
void archive_text(Common& common, NObjectType& nobject, Archive& ar)
{
	ar.obj(0, [&] {
		#define I(n) ar.i32(#n, nobject.n);
		#define B(n) ar.b(#n, nobject.n);
		#define S(n) ar.str(#n, nobject.n);
		#define NObj(n) ar.ref(#n, nobject.n, ObjectResolver<NObjectType>(common, common.nobjectTypes));
		#define SObj(n) ar.ref(#n, nobject.n, ObjectResolver<SObjectType>(common, common.sobjectTypes));

		B(wormExplode)
		B(explGround)
		B(wormDestroy)
		B(drawOnMap)
		B(affectByExplosions)
		B(bloodTrail)

		I(detectDistance)
		I(gravity)
		I(speed)
		I(speedV)
		I(distribution)
		I(blowAway)
		I(bounce)
		I(hitDamage)
		I(bloodOnHit)
		I(startFrame)
		I(numFrames)
		I(colorBullets)
		SObj(createOnExp)	
		I(dirtEffect)
		I(splinterAmount)
		I(splinterColour)
		NObj(splinterType)
		I(bloodTrailDelay)
		SObj(leaveObj)
		I(leaveObjDelay)
		I(timeToExplo)
		I(timeToExploV)
	
		#undef I
		#undef B
		#undef S
		#undef NObj
		#undef SObj
	});
}

template<typename Archive>
void archive_text(Common& common, SObjectType& sobject, Archive& ar)
{
	ar.obj(0, [&] {
		#define I(n) ar.i32(#n, sobject.n);
		#define B(n) ar.b(#n, sobject.n);
		#define S(n) ar.str(#n, sobject.n);

		B(shadow)
		I(startSound)
		I(numSounds)
		I(animDelay)
		I(startFrame)
		I(numFrames)
		I(detectRange)
		I(damage)
		I(blowAway)
		I(shake)
		I(flash)
		I(dirtEffect)

		#undef I
		#undef B
		#undef S
	});
}

template<typename Archive>
void archive_text(Common& common, Weapon& weapon, Archive& ar)
{
	ar.obj(0, [&] {
		#define I(n) ar.i32(#n, weapon.n);
		#define B(n) ar.b(#n, weapon.n);
		#define S(n) ar.str(#n, weapon.n);
		#define NObj(n) ar.ref(#n, weapon.n, ObjectResolver<NObjectType>(common, common.nobjectTypes));
		#define SObj(n) ar.ref(#n, weapon.n, ObjectResolver<SObjectType>(common, common.sobjectTypes));

		S(name)

		B(affectByWorm)
		B(shadow)
		B(laserSight)
		B(playReloadSound)
		B(wormExplode)
		B(explGround)
		B(wormCollide)
		B(collideWithObjects)
		B(affectByExplosions)
		B(loopAnim)
		I(detectDistance)
		I(blowAway)
		I(gravity)
		I(launchSound)
		I(loopSound)
		I(exploSound)
		I(speed)
		I(addSpeed)
		I(distribution)
		I(parts)
		I(recoil)
		I(multSpeed)
		I(delay)
		I(loadingTime)
		I(ammo)
		I(dirtEffect)
		I(leaveShells)
		I(leaveShellDelay)
		I(fireCone)
		I(bounce)
		I(timeToExplo)
		I(timeToExploV)
		I(hitDamage)
		I(bloodOnHit)
		I(startFrame)
		I(numFrames)
		I(shotType)
		I(colorBullets)
		I(splinterAmount)
		I(splinterColour)
		NObj(splinterType)
		I(splinterScatter)
		SObj(objTrailType)
		I(objTrailDelay)
		I(partTrailType)
		NObj(partTrailObj)
		I(partTrailDelay)
		SObj(createOnExp)

		#undef I
		#undef B
		#undef S
		#undef NObj
		#undef SObj
	});
}

template<typename Archive>
void archive_text(Common& common, Archive& ar)
{
	ar.obj("types", [&] {

		ar.arr("sounds", common.sounds, [&] (SfxSample& s) {
			ar.str(0, s.name);
		});

		ar.arr("weapons", common.weapons, [&] (Weapon& w) {
			ar.str(0, w.idStr);
		});

		ar.arr("nobjects", common.nobjectTypes, [&] (NObjectType& n) {
			ar.str(0, n.idStr);
		});

		ar.arr("sobjects", common.sobjectTypes, [&] (SObjectType& s) {
			ar.str(0, s.idStr);
		});

	});

	ar.obj("constants", [&] {
		int bonusIndexes[2] = {0, 1};

		ar.array_obj("bonuses", bonusIndexes, [&] (int idx) {
			ar.i32("timer", common.bonusRandTimer[idx][0]);
			ar.i32("timerV", common.bonusRandTimer[idx][1]);
			ar.i32("frame", common.bonusFrames[idx]);
			ar.ref("sobj", common.bonusSObjects[idx], ObjectResolver<SObjectType>(common, common.sobjectTypes));
		});

		ar.array_obj("textures", common.textures, [&] (Texture& tx) {
			ar.i32("mframe", tx.mFrame);
			ar.i32("rframe", tx.rFrame);
			ar.i32("sframe", tx.sFrame);
			ar.b("ndrawback", tx.nDrawBack);
		});

		ar.array_obj("colorAnim", common.colorAnim, [&] (ColourAnim& ca) {
			ar.i32("from", ca.from);
			ar.i32("to", ca.to);
		});

		ar.arr("materials", common.materials, [&] (Material& m) {
			int f = m.flags;
			ar.i32(0, f);
			m.flags = (uint8_t)(f & 0xff);
		});

		char const* names[7] = {
			"up", "down", "left", "right",
			"fire", "change", "jump"
		};

		ar.obj("aiparams", [&] () {
			for (int i = 0; i < 7; ++i)
			{
				ar.obj(names[i], [&] () {
					ar.i32("on", common.aiParams.k[1][i]);
					ar.i32("off", common.aiParams.k[0][i]);
				});
			}
		});
		

		#define A(n) ar.i32(#n, common.C[C##n]);
		LIERO_CDEFS(A)
		#undef A
	});

	ar.obj("texts", [&] {
		#define A(n) ar.str(#n, common.S[S##n]);
		LIERO_SDEFS(A)
		#undef A
	});

	ar.obj("hacks", [&] {
		#define A(n) ar.b(#n, common.H[H##n]);
		LIERO_HDEFS(A)
		#undef A
	});

	
}

struct OctetTextWriter : gvl::octet_writer
{
	OctetTextWriter(std::string const& path)
	: gvl::octet_writer(gvl::sink(new gvl::file_bucket_pipe(path.c_str(), "wb")))
	, w(*this)
	{
	}

	gvl::toml::writer<gvl::octet_writer> w;
};

struct OctetTextReader : gvl::octet_reader
{
	OctetTextReader(std::string const& path)
	: gvl::octet_reader(gvl::to_source(new gvl::file_bucket_pipe(path.c_str(), "rb")))
	, r(*this)
	{
	}

	gvl::toml::reader<gvl::octet_reader> r;
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

void Common::save(std::string const& path)
{
	OctetTextWriter textWriter(joinPath(path, "tc.txt"));
	archive_text(*this, textWriter.w);

	for (auto& s : sounds)
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
			
		for (auto& s : s.originalData)
			w.put(s + 128);
			
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
			writeSpriteTga(w, smallSprites, exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "large.tga").c_str(), "wb")));
			writeSpriteTga(w, largeSprites, exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "text.tga").c_str(), "wb")));
			writeSpriteTga(w, textSprites, exepal);
		}

		{
			gvl::octet_writer w(gvl::sink(new gvl::file_bucket_pipe(joinPath(dir, "font.tga").c_str(), "wb")));

			std::vector<uint8_t> data(font.chars.size() * 7 * 8, 10);
			for (std::size_t i = 0; i < font.chars.size(); ++i)
			{
				Font::Char& ch = font.chars[i];
				uint8_t* dest = &data[i * 7 * 8];

				for (std::size_t y = 0; y < 8; ++y)
				for (std::size_t x = 0; x < ch.width; ++x)
				{
					dest[y * 7 + x] = ch.data[y * 7 + x] ? 50 : 0;
				}
			}
			writeSpriteTga(w, 7, (int)font.chars.size() * 8, &data[0], exepal);
		}
	}

	for (auto& w : weapons)
	{
		std::string dir(joinPath(path, "weapons/"));
		create_directories(dir);

		OctetTextWriter wWriter(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, wWriter.w);
	}

	for (auto& w : nobjectTypes)
	{
		std::string dir(joinPath(path, "nobjects/"));
		create_directories(dir);

		OctetTextWriter nWriter(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, nWriter.w);
	}

	for (auto& w : sobjectTypes)
	{
		std::string dir(joinPath(path, "sobjects/"));
		create_directories(dir);

		OctetTextWriter sWriter(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, sWriter.w);
	}
}

inline uint32_t quad(char a, char b, char c, char d)
{
	return (uint32_t)a + ((uint32_t)b << 8) + ((uint32_t)c << 16) + ((uint32_t)d << 24);
}

void Common::load(std::string const& path)
{
	OctetTextReader textReader(joinPath(path, "tc.txt"));
	archive_text(*this, textReader.r);

	for (auto& s : sounds)
	{
		std::string dir(joinPath(path, "sounds/"));

		gvl::octet_reader r(gvl::to_source(new gvl::file_bucket_pipe(joinPath(dir, s.name + ".wav").c_str(), "rb")));

		if (gvl::read_uint32_le(r) == quad('R', 'I', 'F', 'F'))
		{
			std::size_t roundedSize = gvl::read_uint32_le(r) + 8;

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
		
				for (auto& s : s.originalData)
					s = r.get() - 128;

				s.sound = sfx_new_sound(dataSize * 2);

				s.createSound();
			}
		}
	}

	{
		std::string dir(joinPath(path, "sprites/"));

		largeSprites.allocate(16, 16, 110);
		smallSprites.allocate(7, 7, 130);
		textSprites.allocate(4, 4, 26);

		{
			gvl::octet_reader r(gvl::to_source(new gvl::file_bucket_pipe(joinPath(dir, "small.tga").c_str(), "rb")));
			readSpriteTga(r, smallSprites, &exepal);
		}

		{
			gvl::octet_reader r(gvl::to_source(new gvl::file_bucket_pipe(joinPath(dir, "large.tga").c_str(), "rb")));
			readSpriteTga(r, largeSprites, 0);
		}

		{
			gvl::octet_reader r(gvl::to_source(new gvl::file_bucket_pipe(joinPath(dir, "text.tga").c_str(), "rb")));
			readSpriteTga(r, textSprites, 0);
		}

		{
			gvl::octet_reader r(gvl::to_source(new gvl::file_bucket_pipe(joinPath(dir, "font.tga").c_str(), "rb")));

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
		std::string dir(joinPath(path, "weapons/"));

		OctetTextReader wReader(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, wReader.r);
	}

	for (auto& w : nobjectTypes)
	{
		std::string dir(joinPath(path, "nobjects/"));

		OctetTextReader nReader(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, nReader.r);
	}

	for (auto& w : sobjectTypes)
	{
		std::string dir(joinPath(path, "sobjects/"));

		OctetTextReader sReader(joinPath(dir, w.idStr + ".txt"));
		archive_text(*this, w, sReader.r);
	}

	precompute();
}

#include "common_exereader.hpp"

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

Common::Common(FsNode const& path, std::string const& exeName)
{
	ReaderFile exe = (path / exeName).read();

	// TODO: Some TCs change the name of the .SND or .CHR for some reason.
	// We could read that name from the exe to make them work.
	ReaderFile gfx = (path / "LIERO.CHR").read();
	ReaderFile snd = (path / "LIERO.SND").read();

	loadFromExe(*this, exe, gfx, snd);

	precompute();
}

std::string Common::guessName() const
{
	std::string const& cp = S[SCopyright2];
	auto p = cp.find('(');
	if (p == std::string::npos)
		p = cp.size();

	while (p > 0 && cp[p] == ' ')
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