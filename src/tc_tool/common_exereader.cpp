#include "common_exereader.hpp"

#include <gvl/io2/convert.hpp>
#include <gvl/io2/fstream.hpp>
#include <cctype>
#include "game/reader.hpp"
#include "game/rand.hpp"
#include "game/filesystem.hpp"

int CSint32desc[][3] =
{
	{CNRInitialLength, 0x32D7, 0x32DD},
	{CNRAttachLength, 0xA679, 0xA67F},

	{0, -1, -1}
};

int CSint24desc[][3] =
{
	{CMinBounceUp, 0x3B7D, 0x3B74},
	{CMinBounceDown, 0x3B00, 0x3AF7},
	{CMinBounceLeft, 0x3A83, 0x3A7A},
	{CMinBounceRight, 0x3A06, 0x39FD},
	{CWormGravity, 0x3BDE, 0x3BD7},
	{CWalkVelLeft, 0x3F97, 0x3F9D},
	{CMaxVelLeft, 0x3F8C, 0x3F83},
	{CWalkVelRight, 0x4018, 0x401E},
	{CMaxVelRight, 0x400D, 0x4004},
	{CJumpForce, 0x3327, 0x332D},
	{CMaxAimVelLeft, 0x30F2, 0x30E9},
	{CAimAccLeft, 0x30FD, 0x3103},
	{CMaxAimVelRight, 0x311A, 0x3111},
	{CAimAccRight, 0x3125, 0x312B},
	{CNinjaropeGravity, 0xA895, 0xA89B},
	{CNRMinLength, 0x3206, 0x31FD},
	{CNRMaxLength, 0x3229, 0x3220},

	{CBonusGravity, 0x72C3, 0x72C9},
	{CBObjGravity, 0x744A, 0x7450},



	// WormFloat hack
	{CWormFloatPower, 0x29DB, 0x29E1},

	{0, -1, -1}
};

int CUint16desc[][2] =
{
	{CBloodLimit, 0xE686},

	{0, -1}
};

int CSint16desc[][2] =
{
	{CWormFricMult, 0x39BD},
	{CWormFricDiv, 0x39C7},
	{CWormMinSpawnDistLast, 0x242E},
	{CWormMinSpawnDistEnemy, 0x244B},
	{CWormSpawnRectX, 0x4913},
	{CWormSpawnRectY, 0x4925},
	{CWormSpawnRectW, 0x490B},
	{CWormSpawnRectH, 0x491D},
	{CAimFricMult, 0x3003},
	{CAimFricDiv, 0x300D},

	{CNRThrowVelX, 0x329B},
	{CNRThrowVelY, 0x32BF},
	{CNRForceShlX, 0xA8AD},
	{CNRForceDivX, 0xA8B7},
	{CNRForceShlY, 0xA8DA},
	{CNRForceDivY, 0xA8E4},
	{CNRForceLenShl, 0xA91E},

	{CBonusBounceMul, 0x731F},
	{CBonusBounceDiv, 0x7329},
	{CBonusFlickerTime, 0x87B8},

	{CBonusDropChance, 0xBECA},
	{CSplinterLarpaVelDiv, 0x677D},
	{CSplinterCracklerVelDiv, 0x67D0},


	// WormFloat hack
	{CWormFloatLevel, 0x29D3},

	// BonusSpawnRect hack
	{CBonusSpawnRectX, 0x2319},
	{CBonusSpawnRectY, 0x2327},
	{CBonusSpawnRectW, 0x2311}, // This is used even when the hack isn't enabled
	{CBonusSpawnRectH, 0x231F}, // -==-

	{0, -1}
};

int CUint8desc[][2] =
{
	{CAimMaxRight, 0x3030},
	{CAimMinRight, 0x304A},
	{CAimMaxLeft, 0x3066},
	{CAimMinLeft, 0x3080},
	{CNRColourBegin, 0x10FD2},
	{CNRColourEnd, 0x11069},
	{CBonusExplodeRisk, 0x2DB2},
	{CBonusHealthVar, 0x2D56},
	{CBonusMinHealth, 0x2D5D},
	{CLaserWeapon, 0x7255},

	{CFirstBloodColour, 0x2388},
	{CNumBloodColours, 0x2381},

	{CRemExpObject, 0x8F8B},

	{0, -1}
};

int CSint8desc[][2] =
{
	{CNRPullVel, 0x31D0},
	{CNRReleaseVel, 0x31F0},

	// FallDamage hack
	{CFallDamageRight, 0x3A0E},
	{CFallDamageLeft, 0x3A8B},
	{CFallDamageDown, 0x3B08},
	{CFallDamageUp, 0x3B85},

	{CBloodStepUp, 0xE67B},
	{CBloodStepDown, 0xE68E},

	{0, -1}
};

int Sstringdesc[][2] =
{
	{SInitSound, 0x177F},
	{SLoadingSounds, 0x18F2},

	{SInit_BaseIO, 0x17DD},
	{SInit_IRQ, 0x17E5},
	{SInit_DMA8, 0x17EE},
	{SInit_DMA16, 0x17F8},

	{SInit_DSPVersion, 0x181E},
	{SInit_Colon, 0x182B},
	{SInit_16bit, 0x182F},
	{SInit_Autoinit, 0x1840},

	{SInit_XMSSucc, 0x189D},

	{SInit_FreeXMS, 0x18C5},
	{SInit_k, 0x18D8},

	{SLoadingAndThinking, 0xFB92},
	{SOK, 0xFBA8},
	{SOK2, 0x190E},
	{SPressAnyKey, 0xFBAB},

	{SCommittedSuicideMsg, 0xE70C},
	{SKilledMsg, 0xE71F},
	{SYoureIt, 0x75C5},

	// Pascal strings
	{SRandom, 0xD6E3},
	{SRandom2, 0xD413},
	{SRegenLevel, 0xD41A},
	{SReloadLevel, 0xD42D},
	{SCopyright, 0xFB60},
	{SCopyright2, 0xE693},
	{SSelWeap, 0xA9C0},
	{SLevelRandom, 0xA9D5},
	{SLevelIs1, 0xA9E3},
	{SLevelIs2, 0xA9EC},
	{SRandomize, 0xA9F4},
	{SDone, 0xA9EE},
	{SReloading, 0x7583},
	{SPressFire, 0x7590},
	{SKills, 0x75A4},
	{SLives, 0x75AC},
	{SSelLevel, 0xD6F2},
	{SWeapon, 0xD700},
	{SAvailability, 0xD707},
	{SNoWeaps, 0xD714},

	{0, -1}
};

struct HackDesc
{
	int which;
	int (*indicators)[2];
};

int hFallDamageInd[][2] =
{
	{0x3A0A, 0x26},
	{0x3A87, 0x26},
	{0x3B04, 0x26},
	{0x3B81, 0x26},

	{-1, 0}
};

int hBonusReloadOnlyInd[][2] =
{
	{0x2DB1, 0xEB}, // We check one byte only, because ProMode has a silly jump destination

	{-1, 0}
};

int hBonusSpawnRectInd[][2] =
{
	{0x2318, 0x05}, // These are the first bytes of the add instructions that offset the spawn
	{0x2323, 0x05},

	{-1, 0}
};

int hBonusOnlyHealthInd[][2] =
{
	{0x228B, 0xB0},
	{0x228C, 0x02},

	{-1, 0}
};

int hBonusOnlyWeaponInd[][2] =
{
	{0x228B, 0xB0},
	{0x228C, 0x01},

	{-1, 0}
};

int hBonusDisableInd[][2] =
{
	{0xBED3, 0xEB},

	{-1, 0}
};

int hWormFloatInd[][2] =
{
	{0x29D7, 0x26}, // 0x26 is the first byte of the sub instruction
	{0x29DA, 0x34}, // 0x34 is the offset to part of velY of the worm

	{-1, 0}
};

int hRemExpInd[][2] =
{
	// Start of the mov instruction that zeroes the timeout counter
	{0x8fc9, 0x26},
	{0x8fca, 0xc7},
	{-1, 0}
};

int hSignedRecoilInd[][2] =
{
	{0x38AC, 0x98},
	{0x38EC, 0x98},
	{-1, 0}
};

int hAirJumpInd[][2] =
{
	{0x3313, 0xEB},
	{0x3314, 0x06},
	{-1, 0}
};

int hMultiJumpInd[][2] =
{
	{0x331B, 0xEB},
	{0x331C, 0x06},
	{-1, 0}
};

HackDesc Hhackdesc[] =
{
	{HFallDamage, hFallDamageInd},
	{HBonusReloadOnly, hBonusReloadOnlyInd},
	{HBonusSpawnRect, hBonusSpawnRectInd},
	{HWormFloat, hWormFloatInd},
	{HBonusOnlyHealth, hBonusOnlyHealthInd},
	{HBonusOnlyWeapon, hBonusOnlyWeaponInd},
	{HBonusDisable, hBonusDisableInd},
	{HRemExp, hRemExpInd},
	{HSignedRecoil, hSignedRecoilInd},
	{HAirJump, hAirJumpInd},
	{HMultiJump, hMultiJumpInd},
	{0, 0}
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
	"Particle, disappearing",
	"Particle, small damage",
	"Particle, medium damage",
	"Particle, larger damage",
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

std::string toId(std::string const& name)
{
	std::string ret;
	for (char c : name)
	{
		if ((uint8_t)c >= 128 || !std::isalnum((uint8_t)c))
			ret += '_';
		else
			ret += std::tolower((uint8_t)c);
	}
	return ret;
}

inline std::string readPascalString(ReaderFile& f)
{
	unsigned char length = f.get();

	char txt[256];
	f.get(reinterpret_cast<uint8_t*>(txt), length);
	return std::string(txt, length);
}

inline std::string readPascalString(ReaderFile& f, unsigned char fieldLen)
{
	char txt[256];
	f.get(reinterpret_cast<uint8_t*>(txt), fieldLen);

	unsigned char length = static_cast<unsigned char>(txt[0]);
	return std::string(txt + 1, length);
}

inline std::string readPascalStringAt(ReaderFile& f, size_t location)
{
	f.seekg(location);
	return readPascalString(f);
}

void loadConstants(Common& common, ReaderFile& exe)
{
	for(int i = 0; CSint32desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint32desc[i][1]);
		int32_t a = (int32_t)gvl::read_uint16_le(exe);
		exe.seekg(CSint32desc[i][2]);
		int32_t b = (int16_t)gvl::read_uint16_le(exe);
		common.C[CSint32desc[i][0]] = a + (b << 16);
	}

	for(int i = 0; CSint24desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint24desc[i][1]);
		int32_t a = (int32_t)gvl::read_uint16_le(exe);
		exe.seekg(CSint24desc[i][2]);
		int32_t b = (int8_t)exe.get();
		common.C[CSint24desc[i][0]] = a + (b << 16);
	}

	for(int i = 0; CSint16desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint16desc[i][1]);
		common.C[CSint16desc[i][0]] = (int16_t)gvl::read_uint16_le(exe);
	}

	for(int i = 0; CUint16desc[i][1] >= 0; ++i)
	{
		exe.seekg(CUint16desc[i][1]);
		common.C[CUint16desc[i][0]] = gvl::read_uint16_le(exe);
	}

	for(int i = 0; CSint8desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint8desc[i][1]);
		common.C[CSint8desc[i][0]] = (int8_t)exe.get();
	}

	for(int i = 0; CUint8desc[i][1] >= 0; ++i)
	{
		exe.seekg(CUint8desc[i][1]);
		common.C[CUint8desc[i][0]] = exe.get();
	}

	for(int i = 0; Sstringdesc[i][1] >= 0; ++i)
	{
		common.S[Sstringdesc[i][0]] = readPascalStringAt(exe, Sstringdesc[i][1]);
	}

	for(int i = 0; Hhackdesc[i].indicators; ++i)
	{
		int (*ind)[2] = Hhackdesc[i].indicators;
		bool active = true;
		for(; (*ind)[0] >= 0; ++ind)
		{
			exe.seekg((*ind)[0]);
			int b = exe.get();
			if(b != (*ind)[1])
			{
				active = false;
				break;
			}
		}

		common.H[Hhackdesc[i].which] = active;
	}
}


void loadPalette(Common& common, ReaderFile& exe)
{
	exe.seekg(132774);

	for(int i = 0; i < 256; ++i)
	{
		unsigned char rgb[3];
		exe.get(reinterpret_cast<uint8_t*>(rgb), 3);

		common.exepal.entries[i].r = rgb[0] & 63;
		common.exepal.entries[i].g = rgb[1] & 63;
		common.exepal.entries[i].b = rgb[2] & 63;
	}

	exe.seekg(0x1AF0C);
	for(int i = 0; i < 4; ++i)
	{
		common.colorAnim[i].from = exe.get();
		common.colorAnim[i].to = exe.get();
	}
}


void loadMaterials(Common& common, ReaderFile& exe)
{
	exe.seekg(0x01C2E0);

	for(int i = 0; i < 256; ++i)
	{
		common.materials[i].flags = 0;
	}

	unsigned char bits[32];

	for(int i = 0; i < 5; ++i)
	{
		exe.get(reinterpret_cast<uint8_t*>(bits), 32);

		for(int j = 0; j < 256; ++j)
		{
			int bit = ((bits[j >> 3] >> (j & 7)) & 1);
			common.materials[j].flags |= bit << i;
		}
	}

	exe.seekg(0x01AEA8);

	exe.get(reinterpret_cast<uint8_t*>(bits), 32);

	for(int j = 0; j < 256; ++j)
	{
		int bit = ((bits[j >> 3] >> (j & 7)) & 1);
		common.materials[j].flags |= bit << 5;
	}
}

struct Read32
{
	static inline int32_t run(ReaderFile& f)
	{
		return (int32_t)gvl::read_uint32_le(f);
	}
};

struct Read16
{
	static inline int32_t run(ReaderFile& f)
	{
		return (int32_t)(int16_t)gvl::read_uint16_le(f);
	}
};

struct Read8
{
	static inline int32_t run(ReaderFile& f)
	{
		return f.get();
	}
};

struct ReadBool
{
	static inline bool run(ReaderFile& f)
	{
		return f.get() != 0;
	}
};

template<typename T>
struct Dec
{
	static inline int32_t run(ReaderFile& f)
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

template<typename Reader, typename T, typename U>
inline void readMembers(ReaderFile& f, std::vector<T>& arr, U (T::*mem))
{
	for(int i = 0; i < arr.size(); ++i)
	{
		(arr[i].*mem) = Reader::run(f);
	}
}

void loadWeapons(Common& common, ReaderFile& exe)
{
	exe.seekg(112806);

	readMembers<Read8>(exe, common.weapons, &Weapon::detectDistance);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::affectByWorm);
	readMembers<Read8>(exe, common.weapons, &Weapon::blowAway);

	exe.seekg(112966);
	readMembers<Read16>(exe, common.weapons, &Weapon::gravity);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::shadow);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::laserSight);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::launchSound);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::loopSound);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::exploSound);
	readMembers<Read16>(exe, common.weapons, &Weapon::speed);
	readMembers<Read16>(exe, common.weapons, &Weapon::addSpeed);
	readMembers<Read16>(exe, common.weapons, &Weapon::distribution);
	readMembers<Read8>(exe, common.weapons, &Weapon::parts);
	readMembers<Read8>(exe, common.weapons, &Weapon::recoil);
	readMembers<Read16>(exe, common.weapons, &Weapon::multSpeed);
	readMembers<Read16>(exe, common.weapons, &Weapon::delay);
	readMembers<Read16>(exe, common.weapons, &Weapon::loadingTime);
	readMembers<Read8>(exe, common.weapons, &Weapon::ammo);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::createOnExp);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::dirtEffect);
	readMembers<Read8>(exe, common.weapons, &Weapon::leaveShells);
	readMembers<Read8>(exe, common.weapons, &Weapon::leaveShellDelay);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::playReloadSound);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::wormExplode);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::explGround);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::wormCollide);
	readMembers<Read8>(exe, common.weapons, &Weapon::fireCone);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::collideWithObjects);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::affectByExplosions);
	readMembers<Read8>(exe, common.weapons, &Weapon::bounce);
	readMembers<Read16>(exe, common.weapons, &Weapon::timeToExplo);
	readMembers<Read16>(exe, common.weapons, &Weapon::timeToExploV);
	readMembers<Read8>(exe, common.weapons, &Weapon::hitDamage);
	readMembers<Read8>(exe, common.weapons, &Weapon::bloodOnHit);
	readMembers<Read16>(exe, common.weapons, &Weapon::startFrame);
	readMembers<Read8>(exe, common.weapons, &Weapon::numFrames);
	readMembers<ReadBool>(exe, common.weapons, &Weapon::loopAnim);
	readMembers<Read8>(exe, common.weapons, &Weapon::shotType);
	readMembers<Read8>(exe, common.weapons, &Weapon::colorBullets);
	readMembers<Read8>(exe, common.weapons, &Weapon::splinterAmount);
	readMembers<Read8>(exe, common.weapons, &Weapon::splinterColour);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::splinterType);
	readMembers<Read8>(exe, common.weapons, &Weapon::splinterScatter);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::objTrailType);
	readMembers<Read8>(exe, common.weapons, &Weapon::objTrailDelay);
	readMembers<Read8>(exe, common.weapons, &Weapon::partTrailType);
	readMembers<Dec<Read8> >(exe, common.weapons, &Weapon::partTrailObj);
	readMembers<Read8>(exe, common.weapons, &Weapon::partTrailDelay);

	exe.seekg(0x1B676);
	for(int i = 0; i < 40; ++i)
	{
		common.weapons[i].name = readPascalString(exe, 14);
		common.weapons[i].idStr = toId(common.weapons[i].name);
		common.weapons[i].id = i;
		common.weapons[i].chainExplosion = i == 34;
	}

	// Special objects
	exe.seekg(115218);
	readMembers<Dec<Read8> >(exe, common.sobjectTypes, &SObjectType::startSound);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::numSounds);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::animDelay);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::startFrame);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::numFrames);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::detectRange);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::damage);
	readMembers<Read32>(exe, common.sobjectTypes, &SObjectType::blowAway); // blowAway has 13 slots, not 14. The last value will overlap with shadow.

	exe.seekg(115368);
	readMembers<ReadBool>(exe, common.sobjectTypes, &SObjectType::shadow);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::shake);
	readMembers<Read8>(exe, common.sobjectTypes, &SObjectType::flash);
	readMembers<Dec<Read8> >(exe, common.sobjectTypes, &SObjectType::dirtEffect);

	for(int i = 0; i < 14; ++i) // TODO: Unhardcode
	{
		common.sobjectTypes[i].id = i;
	}

	exe.seekg(111430);

	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::detectDistance);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::gravity);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::speed);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::speedV);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::distribution);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::blowAway);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::bounce);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::hitDamage);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::wormExplode);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::explGround);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::wormDestroy);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::bloodOnHit);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::startFrame);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::numFrames);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::drawOnMap);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::colorBullets);
	readMembers<Dec<Read8> >(exe, common.nobjectTypes, &NObjectType::createOnExp);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::affectByExplosions);
	readMembers<Dec<Read8> >(exe, common.nobjectTypes, &NObjectType::dirtEffect);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::splinterAmount);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::splinterColour);
	readMembers<Dec<Read8> >(exe, common.nobjectTypes, &NObjectType::splinterType);
	readMembers<ReadBool>(exe, common.nobjectTypes, &NObjectType::bloodTrail);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::bloodTrailDelay);
	readMembers<Dec<Read8> >(exe, common.nobjectTypes, &NObjectType::leaveObj);
	readMembers<Read8>(exe, common.nobjectTypes, &NObjectType::leaveObjDelay);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::timeToExplo);
	readMembers<Read16>(exe, common.nobjectTypes, &NObjectType::timeToExploV);

	for(int i = 0; i < 24; ++i) // TODO: Unhardcode
	{
		common.nobjectTypes[i].id = i;
	}
}

void loadTextures(Common& common, ReaderFile& exe)
{
	exe.seekg(0x1C208);
	readMembers<ReadBool>(exe, common.textures, &Texture::nDrawBack);
	exe.seekg(0x1C1EA);
	readMembers<Read8>(exe, common.textures, &Texture::mFrame);
	exe.seekg(0x1C1F4);
	readMembers<Read8>(exe, common.textures, &Texture::sFrame);
	exe.seekg(0x1C1FE);
	readMembers<Read8>(exe, common.textures, &Texture::rFrame);
}

void loadOthers(Common& common, ReaderFile& exe)
{
	exe.seekg(0x1C1E2);

	for(int i = 0; i < 2; ++i)
	for(int j = 0; j < 2; ++j)
		common.bonusRandTimer[j][i] = gvl::read_uint16_le(exe);

	exe.seekg(0x1AEEE + 2);

	for(int i = 0; i < 2; ++i)
	for(int j = 0; j < 7; ++j)
		common.aiParams.k[i][j] = gvl::read_uint16_le(exe);

	exe.seekg(0x1C1E0);

	for(int i = 0; i < 2; ++i)
		common.bonusSObjects[i] = exe.get() - 1;
}

void loadSprites(SpriteSet& ss, ReaderFile& f, int width, int height, int count)
{
	assert(width == height); // We only support rectangular sprites right now

	ss.width = width;
	ss.height = height;
	ss.spriteSize = width * height;
	ss.count = count;

	int amount = ss.spriteSize * count;
	ss.data.resize(amount);

	std::vector<uint8_t> temp(amount);

	f.get(&temp[0], amount);

	PalIdx* dest = &ss.data[0];
	uint8_t* src = &temp[0];

	for(int i = 0; i < count; i++)
	{
		for(int x = 0; x < width; ++x)
		{
			for(int y = 0; y < height; ++y)
			{
				dest[x + y*width] = src[y];
			}

			src += height;
		}

		dest += ss.spriteSize;
	}
}

void cropSprites(SpriteSet& sprites, int first, int count, int minX, int minY, int width, int height)
{
	// Crop sprites by clearing pixels outside the desired area.

	int maxX = minX + width - 1;
	int maxY = minY + height - 1;

	for (int i = first; i < first + count; i++)
	{
		Sprite sprite = sprites[i];

		for (int y = 0; y < sprite.height; y++)
		{
			for (int x = 0; x < sprite.width; x++)
			{
				if (x < minX || x > maxX || y < minY || y > maxY)
					sprite.mem[y * sprite.width + x] = 0;
			}
		}
	}
}

void loadGfx(Common& common, ReaderFile& exe, ReaderFile& gfx)
{
	exe.seekg(0x1C1DE);
	common.bonusFrames[0] = exe.get();
	common.bonusFrames[1] = exe.get();

	gfx.seekg(10); // Skip some header

	loadSprites(common.largeSprites, gfx, 16, 16, 110);
	gfx.skip(4); // Extra stuff

	loadSprites(common.smallSprites, gfx, 7, 7, 130);
	gfx.skip(4); // Extra stuff

	loadSprites(common.textSprites, gfx, 4, 4, 26);

	// The original would only render 10x9 pixels of the worm sprites.
	// Cropping the worm sprites here to match the original behavior.
	cropSprites(common.largeSprites, 16, 21, 2, 0, 10, 9);

	Rand rand;

	for(int y = 0; y < 16; ++y)
	for(int x = 0; x < 16; ++x)
	{
		int idx = y * 16 + x;
		common.largeSprites.spritePtr(73)[idx] = rand(4) + 160;
		common.largeSprites.spritePtr(74)[idx] = rand(4) + 160;

		common.largeSprites.spritePtr(87)[idx] = rand(4) + 12;
		common.largeSprites.spritePtr(88)[idx] = rand(4) + 12;

		common.largeSprites.spritePtr(82)[idx] = rand(4) + 94;
		common.largeSprites.spritePtr(83)[idx] = rand(4) + 94;
	}
}

void loadSfx(
	std::vector<SfxSample>& sounds,
	ReaderFile& snd)
{
	int count = gvl::read_uint16_le(snd);

	sounds.clear();

	for(int i = 0; i < count; ++i)
	{
		uint8_t name[9];
		name[8] = 0;
		snd.get(name, 8);

		int offset = gvl::read_uint32_le(snd);
		int length = gvl::read_uint32_le(snd);

		auto oldPos = snd.tellg();

		SfxSample sample(toId(reinterpret_cast<char const*>(name)), length);

		if(length > 0)
		{
			snd.seekg(offset);
			snd.get(&sample.originalData[0], length);

			sample.createSound();
		}

		snd.seekg(oldPos);

		sounds.push_back(std::move(sample));
	}
}

void loadFont(Font& font, ReaderFile& exe)
{
	font.chars.resize(250);

	std::size_t const FontSize = 250 * 8 * 8 + 1;
	std::vector<unsigned char> temp(FontSize);

	exe.seekg(0x1C825);

	exe.get(reinterpret_cast<uint8_t*>(&temp[0]), FontSize);

	for(int i = 0; i < 250; ++i)
	{
		unsigned char* ptr = &temp[i*64 + 1];

		for(int y = 0; y < 8; ++y)
		{
			for(int x = 0; x < 7; ++x)
			{
				font.chars[i].data[y*7 + x] = ptr[y*8 + x];
			}
		}

		font.chars[i].width = ptr[63];
	}
}

void loadFromExe(Common& common, ReaderFile& exe, ReaderFile& gfx, ReaderFile& snd)
{
	common.weapons.resize(40);
	common.nobjectTypes.resize(24);
	common.sobjectTypes.resize(14);

	for (int i = 0; i < 14; ++i)
	{
		common.sobjectTypes[i].idStr = toId(sobjectNames[i]);
	}

	for (int i = 0; i < 24; ++i)
	{
		common.nobjectTypes[i].idStr = toId(nobjectNames[i]);
	}

	loadConstants(common, exe);
	loadFont(common.font, exe);
	loadPalette(common, exe);
	loadMaterials(common, exe);
	loadWeapons(common, exe);
	loadTextures(common, exe);
	loadOthers(common, exe);

	loadGfx(common, exe, gfx);
	loadSfx(common.sounds, snd);
}
