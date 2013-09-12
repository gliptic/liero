#include "constants.hpp"
#include "reader.hpp"
#include "game.hpp" // TODO: We should move Common somewhere else

//#include <iostream>

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

void Common::loadConstantsFromEXE(ReaderFile& exe)
{
	for(int i = 0; CSint32desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint32desc[i][1]);
		int a = readUint16(exe);
		exe.seekg(CSint32desc[i][2]);
		int b = readSint16(exe);
		C[CSint32desc[i][0]] = a + (b << 16);
	}
	
	for(int i = 0; CSint24desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint24desc[i][1]);
		int a = readUint16(exe);
		exe.seekg(CSint24desc[i][2]);
		int b = readSint8(exe);
		C[CSint24desc[i][0]] = a + (b << 16);
	}
	
	for(int i = 0; CSint16desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint16desc[i][1]);
		C[CSint16desc[i][0]] = readSint16(exe);
	}
	
	for(int i = 0; CUint16desc[i][1] >= 0; ++i)
	{
		exe.seekg(CUint16desc[i][1]);
		C[CUint16desc[i][0]] = readUint16(exe);
	}
	
	for(int i = 0; CSint8desc[i][1] >= 0; ++i)
	{
		exe.seekg(CSint8desc[i][1]);
		C[CSint8desc[i][0]] = readSint8(exe);
	}
	
	for(int i = 0; CUint8desc[i][1] >= 0; ++i)
	{
		exe.seekg(CUint8desc[i][1]);
		C[CUint8desc[i][0]] = readUint8(exe);
	}
	
	for(int i = 0; Sstringdesc[i][1] >= 0; ++i)
	{
		S[Sstringdesc[i][0]] = readPascalStringAt(exe, Sstringdesc[i][1]);
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
		
		H[Hhackdesc[i].which] = active;
	}
}
