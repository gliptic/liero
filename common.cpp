#include "common.hpp"

#include "reader.hpp"
#include "rand.hpp"
#include "gfx/blit.hpp"
#include "filesystem.hpp"
#include <gvl/io/convert.hpp>
#include <gvl/io2/fstream.hpp>
#include <map>

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
	"Å",
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
	"Ö",
	"Ä",
	"½",
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

void Common::loadPalette(ReaderFile& exe)
{
	exe.seekg(132774);
	
	exepal.read(exe);

	exe.seekg(0x1AF0C);
	for(int i = 0; i < 4; ++i)
	{
		colorAnim[i].from = readUint8(exe);
		colorAnim[i].to = readUint8(exe);
	}
}


void Common::loadMaterials(ReaderFile& exe)
{
	exe.seekg(0x01C2E0);
	
	for(int i = 0; i < 256; ++i)
	{
		materials[i].flags = 0;
	}
	
	unsigned char bits[32];
	
	for(int i = 0; i < 5; ++i)
	{
		exe.get(reinterpret_cast<uint8_t*>(bits), 32);
		
		for(int j = 0; j < 256; ++j)
		{
			int bit = ((bits[j >> 3] >> (j & 7)) & 1);
			materials[j].flags |= bit << i;
		}
	}
	
	exe.seekg(0x01AEA8);
	
	exe.get(reinterpret_cast<uint8_t*>(bits), 32);
	
	for(int j = 0; j < 256; ++j)
	{
		int bit = ((bits[j >> 3] >> (j & 7)) & 1);
		materials[j].flags |= bit << 5;
	}
}

struct Read32
{
	static inline int run(ReaderFile& f)
	{
		return readSint32(f);
	}
};

struct Read16
{
	static inline int run(ReaderFile& f)
	{
		return readSint16(f);
	}
};

struct Read8
{
	static inline int run(ReaderFile& f)
	{
		return readUint8(f);
	}
};

struct ReadBool
{
	static inline bool run(ReaderFile& f)
	{
		return readUint8(f) != 0;
	}
};

template<typename T>
struct Dec
{
	static inline int run(ReaderFile& f)
	{
		return T::run(f) - 1;
	}
};

template<typename Reader, typename T, int N, typename U>
inline void readMembers(ReaderFile& f, T(&arr)[N], U (T::*mem))
{
	for(int i = 0; i < N; ++i)
	{
		(arr[i].*mem) = Reader::run(f);
	}
}

void Common::loadWeapons(ReaderFile& exe)
{
	exe.seekg(112806);
	
	readMembers<Read8>(exe, weapons, &Weapon::detectDistance);
	readMembers<ReadBool>(exe, weapons, &Weapon::affectByWorm);
	readMembers<Read8>(exe, weapons, &Weapon::blowAway);
	
	for(int i = 0; i < 40; ++i)
	{
		weapOrder[i + 1] = readUint8(exe) - 1;
	}
	
	readMembers<Read16>(exe, weapons, &Weapon::gravity);
	readMembers<ReadBool>(exe, weapons, &Weapon::shadow);
	readMembers<ReadBool>(exe, weapons, &Weapon::laserSight);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::launchSound);
	readMembers<ReadBool>(exe, weapons, &Weapon::loopSound);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::exploSound);
	readMembers<Read16>(exe, weapons, &Weapon::speed);
	readMembers<Read16>(exe, weapons, &Weapon::addSpeed);
	readMembers<Read16>(exe, weapons, &Weapon::distribution);
	readMembers<Read8>(exe, weapons, &Weapon::parts);
	readMembers<Read8>(exe, weapons, &Weapon::recoil);
	readMembers<Read16>(exe, weapons, &Weapon::multSpeed);
	readMembers<Read16>(exe, weapons, &Weapon::delay);
	readMembers<Read16>(exe, weapons, &Weapon::loadingTime);
	readMembers<Read8>(exe, weapons, &Weapon::ammo);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::createOnExp);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::dirtEffect);
	readMembers<Read8>(exe, weapons, &Weapon::leaveShells);
	readMembers<Read8>(exe, weapons, &Weapon::leaveShellDelay);
	readMembers<ReadBool>(exe, weapons, &Weapon::playReloadSound);
	readMembers<ReadBool>(exe, weapons, &Weapon::wormExplode);
	readMembers<ReadBool>(exe, weapons, &Weapon::explGround);
	readMembers<ReadBool>(exe, weapons, &Weapon::wormCollide);
	readMembers<Read8>(exe, weapons, &Weapon::fireCone);
	readMembers<ReadBool>(exe, weapons, &Weapon::collideWithObjects);
	readMembers<ReadBool>(exe, weapons, &Weapon::affectByExplosions);
	readMembers<Read8>(exe, weapons, &Weapon::bounce);
	readMembers<Read16>(exe, weapons, &Weapon::timeToExplo);
	readMembers<Read16>(exe, weapons, &Weapon::timeToExploV);
	readMembers<Read8>(exe, weapons, &Weapon::hitDamage);
	readMembers<Read8>(exe, weapons, &Weapon::bloodOnHit);
	readMembers<Read16>(exe, weapons, &Weapon::startFrame);
	readMembers<Read8>(exe, weapons, &Weapon::numFrames);
	readMembers<ReadBool>(exe, weapons, &Weapon::loopAnim);
	readMembers<Read8>(exe, weapons, &Weapon::shotType);
	readMembers<Read8>(exe, weapons, &Weapon::colorBullets);
	readMembers<Read8>(exe, weapons, &Weapon::splinterAmount);
	readMembers<Read8>(exe, weapons, &Weapon::splinterColour);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::splinterType);
	readMembers<Read8>(exe, weapons, &Weapon::splinterScatter);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::objTrailType);
	readMembers<Read8>(exe, weapons, &Weapon::objTrailDelay);
	readMembers<Read8>(exe, weapons, &Weapon::partTrailType);
	readMembers<Dec<Read8> >(exe, weapons, &Weapon::partTrailObj);
	readMembers<Read8>(exe, weapons, &Weapon::partTrailDelay);
	
	exe.seekg(0x1B676);
	for(int i = 0; i < 40; ++i)
	{
		weapons[i].name = readPascalString(exe, 14);
		weapons[i].id = i;
	}
	
	// Special objects
	exe.seekg(115218);
	readMembers<Dec<Read8> >(exe, sobjectTypes, &SObjectType::startSound);
	//fseek(exe, 115232, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::numSounds);
	//fseek(exe, 115246, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::animDelay);
	//fseek(exe, 115260, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::startFrame);
	//fseek(exe, 115274, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::numFrames);
	//fseek(exe, 115288, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::detectRange);
	//fseek(exe, 115302, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::damage);
	//fseek(exe, 0x1C274, SEEK_SET);
	readMembers<Read32>(exe, sobjectTypes, &SObjectType::blowAway); // blowAway has 13 slots, not 14. The last value will overlap with shadow.

	exe.seekg(115368);
	readMembers<ReadBool>(exe, sobjectTypes, &SObjectType::shadow);
	//fseek(exe, 115382, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::shake);
	//fseek(exe, 115396, SEEK_SET);
	readMembers<Read8>(exe, sobjectTypes, &SObjectType::flash);
	//fseek(exe, 115410, SEEK_SET); // Was 115409
	readMembers<Dec<Read8> >(exe, sobjectTypes, &SObjectType::dirtEffect);
	
	for(int i = 0; i < 14; ++i) // TODO: Unhardcode
	{
		sobjectTypes[i].id = i;
	}
	
	exe.seekg(111430);
	
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::detectDistance);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::gravity);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::speed);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::speedV);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::distribution);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::blowAway);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::bounce);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::hitDamage);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::wormExplode);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::explGround);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::wormDestroy);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::bloodOnHit);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::startFrame);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::numFrames);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::drawOnMap);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::colorBullets);
	readMembers<Dec<Read8> >(exe, nobjectTypes, &NObjectType::createOnExp);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::affectByExplosions);
	readMembers<Dec<Read8> >(exe, nobjectTypes, &NObjectType::dirtEffect);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::splinterAmount);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::splinterColour);
	readMembers<Dec<Read8> >(exe, nobjectTypes, &NObjectType::splinterType);
	readMembers<ReadBool>(exe, nobjectTypes, &NObjectType::bloodTrail);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::bloodTrailDelay);
	readMembers<Dec<Read8> >(exe, nobjectTypes, &NObjectType::leaveObj);
	readMembers<Read8>(exe, nobjectTypes, &NObjectType::leaveObjDelay);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::timeToExplo);
	readMembers<Read16>(exe, nobjectTypes, &NObjectType::timeToExploV);
	
	for(int i = 0; i < 24; ++i) // TODO: Unhardcode
	{
		nobjectTypes[i].id = i;
	}
}

void Common::loadTextures(ReaderFile& exe)
{
	exe.seekg(0x1C208);
	readMembers<ReadBool>(exe, textures, &Texture::nDrawBack);
	exe.seekg(0x1C1EA);
	readMembers<Read8>(exe, textures, &Texture::mFrame);
	exe.seekg(0x1C1F4);
	readMembers<Read8>(exe, textures, &Texture::sFrame);
	exe.seekg(0x1C1FE);
	readMembers<Read8>(exe, textures, &Texture::rFrame);
}

void Common::loadOthers(ReaderFile& exe)
{
	exe.seekg(0x1C1E2);
	
	for(int i = 0; i < 2; ++i)
	for(int j = 0; j < 2; ++j)
		bonusRandTimer[j][i] = readUint16(exe);
		
	exe.seekg(0x1AEEE + 2);
	
	for(int i = 0; i < 2; ++i)
	for(int j = 0; j < 7; ++j)
		aiParams.k[i][j] = readUint16(exe);
		
	exe.seekg(0x1C1E0);
	
	for(int i = 0; i < 2; ++i)
		bonusSObjects[i] = readUint8(exe) - 1;
}

void Common::loadGfx(ReaderFile& exe, ReaderFile& gfx)
{
	exe.seekg(0x1C1DE);
	bonusFrames[0] = readUint8(exe);
	bonusFrames[1] = readUint8(exe);
	
	gfx.seekg(10); // Skip some header
	
	largeSprites.read(gfx, 16, 16, 110);
	gfx.skip(4); // Extra stuff
	
	smallSprites.read(gfx, 7, 7, 130);
	gfx.skip(4); // Extra stuff
	
	textSprites.read(gfx, 4, 4, 26);
	
	Rand rand;
	
	for(int y = 0; y < 16; ++y)
	for(int x = 0; x < 16; ++x)
	{
		int idx = y * 16 + x;
		largeSprites.spritePtr(73)[idx] = rand(4) + 160;
		largeSprites.spritePtr(74)[idx] = rand(4) + 160;
		
		largeSprites.spritePtr(87)[idx] = rand(4) + 12;
		largeSprites.spritePtr(88)[idx] = rand(4) + 12;
		
		largeSprites.spritePtr(82)[idx] = rand(4) + 94;
		largeSprites.spritePtr(83)[idx] = rand(4) + 94;
	}
	
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

void Common::loadSfx(ReaderFile& snd)
{
	int count = readUint16(snd);
	
	sounds.resize(count);
	
	long oldPos = snd.tellg();
	
	for(int i = 0; i < count; ++i)
	{
		snd.seekg(oldPos + 8);
		
		int offset = readUint32(snd);
		int length = readUint32(snd);
		
		oldPos = snd.tellg();
		
		int byteLength = length * 4;

		sounds[i] = sfx_new_sound(byteLength / 2);
		
		int16_t* ptr = reinterpret_cast<int16_t*>(sfx_sound_data(sounds[i]));
		
		std::vector<uint8_t> temp(length);
		
		if(length > 0)
		{
			snd.seekg(offset);
			snd.get(&temp[0], length);

			int prev = ((int8_t)temp[0]) * 30;
			*ptr++ = prev;
		
			for(int j = 1; j < length; ++j)
			{
				int cur = (int8_t)temp[j] * 30;
				*ptr++ = (prev + cur) / 2;
				*ptr++ = cur;
				prev = cur;
			}
		
			*ptr++ = prev;
		}
	}
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

#include <gvl/io/encoding.hpp>

template<typename Writer>
struct TextWriter
{
	TextWriter(Writer& writer)
	: writer(writer)
	, first(true)
	, indent(0)
	{
	}

	bool first;
	int indent;

	void windent()
	{
		writer << '\n';
		for (int i = 0; i < indent; ++i)
		{
			writer << "  ";
		}
	}

	void f(char const* name)
	{
		if (!first)
			writer << ',';
		else
			first = false;
		windent();
		if (name)
			writer << '"' << name << "\": ";
	}

	template<typename F>
	void object(char const* name, F func)
	{
		f(name);
		writer << '{';
		++indent;
		first = true;
		func();
		--indent;
		windent();
		writer << "}";
		first = false;
	}

	template<typename A, typename F>
	void array(char const* name, A& arr, F func)
	{
		f(name);
		writer << '[';
		++indent;
		first = true;
		for (auto& e : arr)
		{
			func(e);
		}
		--indent;
		windent();
		writer << "]";
		first = false;
	}

	void i32(int v)
	{
		writer << v;
	}

	void i32(char const* name, int v)
	{
		f(name);
		i32(v);
	}

	void b(bool v)
	{
		writer << (v ? "true" : "false");
	}

	void b(char const* name, bool v)
	{
		f(name);
		b(v);
	}

	template<typename T, typename ValueToRef, typename RefToValue>
	void ref(char const* name, T const& v, ValueToRef v2r, RefToValue /*r2v*/)
	{
		f(name);
		v2r(*this, v);
	}

	void str(std::string& s)
	{
		writer << '"';
		for (char c : s)
		{
			if (c >= 0x20 && c <= 0x7e)
			{
				if (c == '"' || c == '\\')
					writer << '\\';
				writer << c;
			}
			else
			{
				writer << "\\u";
				gvl::uint_to_ascii_base<16>(writer, (uint8_t)c, 4);
			}
			// TODO: Handle non-printable
		}
		writer << '"';
	}

	void str(char const* name, std::string& s)
	{
		f(name);
		str(s);
	}

	Writer& writer;
};

enum Type
{
	TNull,
	TBool,
	TInteger,
	TString,
	TObject,
	TArray
};

struct Object;
struct Array;
struct String;

using gvl::shared;
using std::vector;
using std::string;
using std::map;
using std::move;

struct Value
{
	Type tt;
	union
	{
		int i;
		shared* s;
	} u;

	Value()
	: tt(TNull)
	{
	}

	Value(int v)
	: tt(TInteger)
	{
		u.i = v;
	}

	Value(bool v)
	: tt(TBool)
	{
		u.i = v;
	}

	Value(Value const& other)
	: tt(other.tt), u(other.u)
	{
		if (tt >= TString)
			u.s->add_ref();
	}

	Value(Value&& other)
	: tt(other.tt), u(other.u)
	{
		other.tt = TNull;
	}

	inline Value(Object&& o);
	inline Value(Array&& a);
	inline Value(String&& s);

	Value& operator=(Value const& other)
	{
		shared* old = tt >= TString ? u.s : 0;
		tt = other.tt;
		u = other.u;
		if (tt >= TString)
			u.s->add_ref();
		if (old) old->release();
		return *this;
	}

	Value& operator=(Value&& other)
	{
		shared* old = tt >= TString ? u.s : 0;
		tt = other.tt;
		u = other.u;
		other.tt = TNull;
		if (old) old->release();
		return *this;
	}

	~Value()
	{
		if (tt >= TString)
			u.s->release();
	}
};

struct String : shared
{
	string s;
};

struct Array : shared
{
	vector<Value> v;
};

struct Object : shared
{
	map<string, Value> f;
};

inline Value::Value(Object&& o)
: tt(TObject)
{
	u.s = new Object(o);
}

inline Value::Value(Array&& a)
: tt(TArray)
{
	u.s = new Array(a);
}

inline Value::Value(String&& s)
: tt(TString)
{
	u.s = new String(s);
}

struct ParseError
{
};

template<typename Reader>
struct TextReader
{
	TextReader(Reader& reader)
	: reader(reader)
	{
	}

	Value root, cur;

	void start()
	{
		cur = value();
		check(0);
	}

	Value f(char const* name)
	{
		if (name)
		{
			if (cur.tt != TObject)
				throw ParseError();
			return ((Object*)cur.u.s)->f.at(name);
		}
		else
		{
			return cur;
		}
	}

	template<typename F>
	void object(char const* name, F func)
	{
		Value parent(cur);
		cur = f(name);
		func();
		cur = move(parent);
	}

	template<typename A, typename F>
	void array(char const* name, A& arr, F func)
	{
		Value parent(cur);
		Value a(f(name));
		if (a.tt != TArray)
			throw ParseError();
		Array& ja = *(Array*)cur.u.s;
		// TODO: arr.resize(ja.v.size());

		auto i = ja.v.begin();
		for (auto& e : arr)
		{
			cur = *i++;
			func(e);
		}

		cur = move(parent);
	}

	void i32(char const* name, int& v)
	{
		Value jv(f(name));
		if (jv.tt != TInteger) throw ParseError();
		v = jv.u.i;
	}

	void b(char const* name, bool& v)
	{
		Value jv(f(name));
		if (jv.tt != TBool) throw ParseError();
		v = jv.u.i != 0;
	}

	template<typename T, typename ValueToRef, typename RefToValue>
	void ref(char const* name, T const& v, ValueToRef /*v2r*/, RefToValue r2v)
	{
		f();
		r2v();
	}

	void str(char const* name, std::string& s)
	{
		Value jv(f(name));
		if (jv.tt != TString) throw ParseError();
		s = ((String*)jv.u.s)->s;
	}

	//

	void next()
	{
		do
			c = reader.get_def();
		while (c == ' ' || c == '\r' || c == '\n' || c == '\t');
	}

	void check(uint8_t e)
	{
		if (c != e)
			throw ParseError();
		next();
	}

	String val_str()
	{
		String s;

		check('"');
		while (c != '"')
		{
			if (c == '\"')
			{
				next();
				if (c == '\\' || c == '/' || c == '"')
				{
					s.s.push_back(c);
				}
				else if (c == 'u')
				{
					uint8_t c0 = reader.get_def();
					uint8_t c1 = reader.get_def();
					uint8_t c2 = reader.get_def();
					uint8_t c3 = reader.get_def();

					// TODO: Make char
				}
				else
				{
					throw ParseError();
				}
			}
			else
			{
				s.s.push_back(c);
			}
		}
		check('"');

		return move(s);
	}

	Value value()
	{
		if (c == '[')
		{
			Array a;

			next();
			
			while (c != ']')
			{
				a.v.emplace_back(value());
				if (c || c == ']')
					break;
				check(',');
			}

			check(']');

			return Value(move(a));
		}
		else if (c == '{')
		{
			Object o;

			next();
			
			while (c != '}')
			{
				string name(str());
				check(':');
				
				o.f.emplace(move(name), value());
				if (c || c == '}')
					break;
				check(',');
			}

			check('}');

			return Value(move(o));
		}
		else if (c == 't')
		{
			next();
			check('r');
			check('u');
			check('e');
			return Value(true);
		}
		else if (c == 'f')
		{
			next();
			check('a');
			check('l');
			check('s');
			check('e');
			return Value(false);
		}
		else if (c == 'n')
		{
			next();
			check('u');
			check('l');
			check('l');
			return Value();
		}
		else if (c == '"')
		{
			return Value(str());
		}
		else if (c == '-' || (c >= '0' && c <= '9'))
		{
			bool neg = false;
			if (c == '-')
			{
				neg = true;
				next();
			}

			int v = 0;

			while (c >= '0' && c <= '9')
			{
				v = (v * 10) + (c - '0');
			}

			return Value(neg ? -v : v);
		}

		throw ParseError();
	}

	Reader& reader;
	uint8_t c;
};

char const* sobjectNames[14] = {
	"Large explosion",
	"Medium explosion",
	"Small explosion",
	"Hellraider smoke",
	"Zimm flash",
	"Nuke smoke",
	"Flashing pixel",
	"Teleport flash",
	"Small explosion, silent",
	"Very small explosion, silent",
	"Medium explosion, smaller",
	"Large explosion, smaller",
	"Medium explosion, bigger",
	"Unknown"
};

char const* nobjectNames[24] = {
	"Worm 1 parts",
	"Worm 2 parts",
	"Particle (disappearing)",
	"Particle (small damage)",
	"Particle (medium damage)",
	"Particle (larger damage)",
	"Blood",
	"Shells",
	"Clusterbomb bombs",
	"Large nukes",
	"Hellraider bullets",
	"Small nukes",
	"Napalm fireballs",
	"Dirt",
	"Chiquitabomb bombs",
	"Grasshopper 1",
	"Grasshopper 2",
	"Grasshopper 3",
	"Grasshopper 4",
	"Grasshopper 5",
	"Flag 1",
	"Flag 2",
	"Grasshopper 6",
	"Grasshopper 7"
};

template<typename Archive>
void archive_text(NObjectType& nobject, Archive& ar)
{
	ar.object(0, [&] {
		#define I(n) ar.i32(#n, nobject.n);
		#define B(n) ar.b(#n, nobject.n);
		#define S(n) ar.str(#n, nobject.n);

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
		I(createOnExp)	
		I(dirtEffect)
		I(splinterAmount)
		I(splinterColour)
		I(splinterType)
		I(bloodTrailDelay)
		I(leaveObj)
		I(leaveObjDelay)
		I(timeToExplo)
		I(timeToExploV)
	
		#undef I
		#undef B
		#undef S
	});
}

template<typename Archive>
void archive_text(SObjectType& sobject, Archive& ar)
{
	ar.object(0, [&] {
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
void archive_text(Weapon& weapon, Archive& ar)
{
	ar.object(0, [&] {
		#define I(n) ar.i32(#n, weapon.n);
		#define B(n) ar.b(#n, weapon.n);
		#define S(n) ar.str(#n, weapon.n);

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
		I(splinterType)
		I(splinterScatter)
		I(objTrailType)
		I(objTrailDelay)
		I(partTrailType)
		I(partTrailObj)
		I(partTrailDelay)

		I(createOnExp)

		#undef I
		#undef B
		#undef S
	});
}

template<typename Archive>
void archive_text(Common& common, Archive& ar)
{
	ar.object(0, [&] {
		ar.object("constants", [&] {
			#define A(n) ar.i32(#n, common.C[C##n]);
			LIERO_CDEFS(A)
			#undef A
		});

		ar.object("texts", [&] {
			#define A(n) ar.str(#n, common.S[S##n]);
			LIERO_SDEFS(A)
			#undef A
		});

		ar.object("hacks", [&] {
			#define A(n) ar.b(#n, common.H[H##n]);
			LIERO_HDEFS(A)
			#undef A
		});

		ar.array("weapons", common.weapons, [&] (Weapon& w) {
			//archive_text(w, ar);
			ar.str(0, w.name);
			// TODO: Set id
		});

		ar.array("nobjects", common.nobjectTypes, [&] (NObjectType& n) {
			//archive_text(n, ar);
			ar.str(0, n.name);
			// TODO: Set id
		});

		ar.array("sobjects", common.sobjectTypes, [&] (SObjectType& s) {
			//archive_text(s, ar);
			ar.str(0, s.name);
			// TODO: Set id
		});
	});
}

struct OctetTextWriter : gvl::octet_writer
{
	OctetTextWriter(string const& path)
	: gvl::octet_writer(gvl::sink(new gvl::file_bucket_source(path.c_str(), "wb")))
	, w(*this)
	{
	}

	TextWriter<gvl::octet_writer> w;
};

struct OctetTextReader : gvl::octet_reader
{
	OctetTextReader(string const& path)
	: gvl::octet_reader(gvl::to_source(new gvl::file_bucket_source(path.c_str(), "rb")))
	, r(*this)
	{
	}

	TextReader<gvl::octet_reader> r;
};

void Common::save(std::string const& path)
{
	{
		OctetTextWriter textWriter(joinPath(path, "tc.txt"));
		archive_text(*this, textWriter.w);

		for (auto& w : weapons)
		{
			OctetTextWriter wWriter(joinPath(path, w.name + ".txt"));
			archive_text(w, wWriter.w);
		}
	}

	{
		OctetTextReader textReader(joinPath(path, "tc.txt"));
		archive_text(*this, textReader.r);
	}
}


Common::Common(std::string const& lieroExe)
{
	ReaderFile& exe = openLieroEXE(lieroExe);
	ReaderFile& gfx = openLieroCHR(lieroExe);
	ReaderFile& snd = openLieroSND(lieroExe);

	for (int i = 0; i < 14; ++i)
		sobjectTypes[i].name = sobjectNames[i];

	for (int i = 0; i < 24; ++i)
		nobjectTypes[i].name = nobjectNames[i];

	loadConstantsFromEXE(exe);
	font.loadFromEXE(exe);
	loadPalette(exe);
	loadMaterials(exe);
	loadWeapons(exe);
	loadTextures(exe);
	loadOthers(exe);

	loadGfx(exe, gfx);
	loadSfx(snd);

    if (false)
	{
		auto path = changeLeaf(lieroExe, "");
		
		save(path);
	}
}