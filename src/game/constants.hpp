#pragma once

#include <string>

// Variables sourced from [constants] in tc.cfg
#define LIERO_CDEFS(_) \
	_(NRInitialLength) \
	_(NRAttachLength) \
	_(MinBounceUp) \
	_(MinBounceDown) \
	_(MinBounceLeft) \
	_(MinBounceRight) \
	_(WormGravity) \
	_(WalkVelLeft) \
	_(MaxVelLeft) \
	_(WalkVelRight) \
	_(MaxVelRight) \
	_(JumpForce) \
	_(MaxAimVelLeft) \
	_(AimAccLeft) \
	_(MaxAimVelRight) \
	_(AimAccRight) \
	_(NinjaropeGravity) \
	_(NRMinLength) \
	_(NRMaxLength) \
	_(BonusGravity) \
	_(WormFricMult) \
	_(WormFricDiv) \
	_(WormMinSpawnDistLast) \
	_(WormMinSpawnDistEnemy) \
	_(WormSpawnRectX) \
	_(WormSpawnRectY) \
	_(WormSpawnRectW) \
	_(WormSpawnRectH) \
	_(AimFricMult) \
	_(AimFricDiv) \
	_(NRThrowVelX) \
	_(NRThrowVelY) \
	_(NRForceShlX) \
	_(NRForceDivX) \
	_(NRForceShlY) \
	_(NRForceDivY) \
	_(NRForceLenShl) \
	_(BonusBounceMul) \
	_(BonusBounceDiv) \
	_(BonusFlickerTime) \
	_(AimMaxRight) \
	_(AimMinRight) \
	_(AimMaxLeft) \
	_(AimMinLeft) \
	_(NRPullVel) \
	_(NRReleaseVel) \
	_(NRColourBegin) \
	_(NRColourEnd) \
	_(BonusExplodeRisk) \
	_(BonusHealthVar) \
	_(BonusMinHealth) \
	_(LaserWeapon) \
	_(FirstBloodColour) \
	_(NumBloodColours) \
	_(BObjGravity) \
	_(BonusDropChance) \
	_(SplinterLarpaVelDiv) \
	_(SplinterCracklerVelDiv) \
	_(BloodStepUp) \
	_(BloodStepDown) \
	_(BloodLimit) \
	_(FallDamageRight) \
	_(FallDamageLeft) \
	_(FallDamageDown) \
	_(FallDamageUp) \
	_(WormFloatLevel) \
	_(WormFloatPower) \
	_(BonusSpawnRectX) \
	_(BonusSpawnRectY) \
	_(BonusSpawnRectW) \
	_(BonusSpawnRectH) \
	_(RemExpObject)

// Text strings sourced from [texts] in tc.cfg
#define LIERO_SDEFS(_) \
	_(InitSound) \
	_(LoadingSounds) \
	_(LoadingAndThinking) \
	_(OK) \
	_(OK2) \
	_(PressAnyKey) \
	_(CommittedSuicideMsg) \
	_(KilledMsg) \
	_(YoureIt) \
	_(Init_BaseIO) \
	_(Init_IRQ) \
	_(Init_DMA8) \
	_(Init_DMA16) \
	_(Init_DSPVersion) \
	_(Init_Colon) \
	_(Init_16bit) \
	_(Init_Autoinit) \
	_(Init_XMSSucc) \
	_(Init_FreeXMS) \
	_(Init_k) \
	_(Random) \
	_(Random2) \
	_(RegenLevel) \
	_(ReloadLevel) \
	_(Copyright) \
	_(Copyright2) \
	_(SelWeap) \
	_(LevelRandom) \
	_(LevelIs1) \
	_(LevelIs2) \
	_(Randomize) \
	_(Done) \
	_(Reloading) \
	_(PressFire) \
	_(Kills) \
	_(Lives) \
	_(SelLevel) \
	_(Weapon) \
	_(Availability) \
	_(NoWeaps) \

// Sound hooks the engine plays directly. Sourced from [sounds] in tc.cfg.
#define LIERO_SOUNDDEFS(_) \
	_(MenuMoveUp)     \
	_(MenuMoveDown)   \
	_(MenuSelect)     \
	_(Bump)           \
	_(Begin)          \
	_(Reloaded)       \
	_(Alive)          \
	_(NinjaropeThrow)

// Boolean values describing which hacks are enabled. Sourced from [hacks] in tc.cfg
#define LIERO_HDEFS(_) \
	_(FallDamage) \
	_(BonusReloadOnly) \
	_(BonusSpawnRect) \
	_(BonusOnlyHealth) \
	_(BonusOnlyWeapon) \
	_(BonusDisable) \
	_(WormFloat) \
	_(RemExp) \
	_(SignedRecoil) \
	_(AirJump) \
	_(MultiJump)

#define DEFENUMS(x) S##x,
#define DEFENUMC(x) C##x,
#define DEFENUMH(x) H##x,
#define DEFENUMSO(x) Sound##x,

enum CONST_DEF_T
{
	LIERO_CDEFS(DEFENUMC)
	/* Maximum quantity of constants in CONST_DEF_T. */
	MaxC
};

enum STRING_DEF_T
{
	LIERO_SDEFS(DEFENUMS)
	/* Maximum quantity of strings in STRING_DEF_T. */
	MaxS
};

enum HACK_DEF_T
{
	LIERO_HDEFS(DEFENUMH)
	/* Maximum quantity of hacks in HACK_DEF_T. */
	MaxH
};

enum SOUND_DEF_T
{
	LIERO_SOUNDDEFS(DEFENUMSO)
	/* Maximum quantity of sound hooks in SOUND_DEF_T. */
	MaxSound
};

#undef DEFENUMS
#undef DEFENUMC
#undef DEFENUMH
#undef DEFENUMSO

#define LC(name) (common.C[C##name])
#define LS(name) (common.S[S##name])


// TODO: Move these to Common
