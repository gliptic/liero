#include "common.hpp"

#include "reader.hpp"
#include "rand.hpp"
#include "gfx/blit.hpp"
#include "filesystem.hpp"
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
	: gvl::octet_writer(gvl::sink(new gvl::file_bucket_source(path.c_str(), "wb")))
	, w(*this)
	{
	}

	gvl::toml::writer<gvl::octet_writer> w;
};

struct OctetTextReader : gvl::octet_reader
{
	OctetTextReader(std::string const& path)
	: gvl::octet_reader(gvl::to_source(new gvl::file_bucket_source(path.c_str(), "rb")))
	, r(*this)
	{
	}

	gvl::toml::reader<gvl::octet_reader> r;
};

void Common::save(std::string const& path)
{
	{
		OctetTextWriter textWriter(joinPath(path, "tc.txt"));
		archive_text(*this, textWriter.w);

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

	if (true)
	{
		OctetTextReader textReader(joinPath(path, "tc.txt"));
		archive_text(*this, textReader.r);

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
	}
}

#include "common_exereader.hpp"

void Common::precompute()
{
	weapOrder.resize(weapons.size());
	for (int i = 0; i < weapons.size(); ++i)
	{
		weapOrder[i] = i;
	}

	std::sort(weapOrder.begin(), weapOrder.end(), [&](int a, int b) {
		return this->weapons[a].name < this->weapons[b].name;
	});
}

Common::Common(std::string const& lieroExe)
{
	loadFromExe(*this, lieroExe);

	precompute();

	if (false)
	{
		auto path = changeLeaf(lieroExe, "");
		
		save(path);
	}
}