#include <gvl/serialization/toml.hpp>
#include "common.hpp"
#include <gvl/io2/stream.hpp>

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
		B(chainExplosion)

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