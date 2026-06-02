#pragma once

#include <string>
#include "exactObjectList.hpp"
#include "math.hpp"

struct Worm;
struct Game;
struct Settings;
struct WormWeapon;

struct Weapon {
  /*
   * ShotType: (Projectile)
   * Normal (0)
   * Type1 (1)
   * Steerable (2)
   * Type2 (3)
   * Laser (4)
   */
  enum { kStNormal, kStdType1, kStSteerable, kStdType2, kStLaser };

  void Fire(Game& game, int angle, fixedvec vel, int speed, fixedvec pos, int owner_idx,
            WormWeapon* ww) const;

  /*
  Additional worm detect distance for the bullet. Affects the distance at which an object hits a
  worm. Add more for "bigger" bullets or things like proximity detonators. Note: this parameter also
  determines starting distance from player (the distance at which the object is created). Note: if
  detectDistance < 0, then the worm will not receive damage from the object and also
  blowAway/wormCollide parameters will not work.
  */
  int detect_distance;
  /*
  Whether the movement of the player affects the initial speed and direction of the object.
  */
  bool affect_by_worm;
  /*
  Force affecting the worm on hit.
  Note: this will also work if the object has "wormCollide" set to false; in such case, the object
  will not disappear but push the worm continuously. Note: works only if the object is moving and
  detectDistance ⩾ 0!
  */
  int blow_away;
  /*
  Gravity of the object. Can also be negative.
  */
  fixed gravity;
  /*
  Whether the object should create a shadow.
  */
  bool shadow;
  /*
  Whether a wObject has the flickering, red laser sight. It cannot be configured in any way, except
  for changing its colour (this requires palette changing though). Note: laser sight is not
  displayed on special rock (undefined) on the map (or after it), however the bullet itself passes
  through such type of material.
  */
  bool laser_sight;
  /*
  Sound played when the weapon is fired. Set -1 for none.
  */
  int launch_sound;
  /*
  Loop sound while fire key is pressed and cut off sound as soon as fire key is released. Buggy.
  */
  bool loop_sound;
  /*
  Sound played when the object explodes. Set -1 for none.
  */
  int explo_sound;
  /*
  Initial speed of the bullet. 600 is about the max playable value for usual weapons.
  Note: if you set too high value for it, the bullet might pass through worms and even through
  thinner walls. Note: this parameter defines also base speed used for missile-type weapons
  ("shotType" = 2). Use "addSpeed" property to define additional speed when pressing "up" button
  while flying.
  */
  int speed;
  /*
  Works in two modes:
  a) for shotType = 3, this is additional speed added each frame. Use it for constant accelerating
  bullets (like in e.g. bazooka). b) for shotType = 2 (directional player-controlled missile), this
  is an additional speed added to the missile when pressing up. It has no impact on other shotType
  (0, 1 and 4).
  */
  fixed add_speed;
  /*
  Spread of the bullet. This works by adding a random direction vector of random length to current
  speed vector of the projectile. Note: if you set its value to > 1 or < -1, then more than 1
  direction vector will be applied, so that the projectiles will have a variable initial velocity
  after firing.
  */
  int distribution;
  /*
  Defines how many objects (particles) the weapon shoots. 1 for pistols, bazookas etc., 20 will be a
  shotgun-type weapon.
  */
  int parts;
  /*
  Pushback force with which the worm is thrown away when firing a weapon.
  */
  int recoil;
  /*
  Speed multiplication each frame. Use it to have a weapon which accelerates or decelerates
  non-linearly, like proxy mine from promode.
  */
  int mult_speed;
  /*
  Delay time (in frames) between individual shots of the same weapon.
  */
  int delay;
  /*
  Reload time.
  */
  int loading_time;
  /*
  Number of shots before the weapon needs to be reloaded.
  */
  int ammo;
  /*
  Which special object (sObject) to use on explosion. Set -1 for none.
  */
  int create_on_exp;
  /*
  Which dirt mask to use on object explosion. Set -1 for none.
  */
  int dirt_effect;
  /*
  Frequency of shells ejected. Set to 0,1,2,3 or 4. 0=never, 1=always, 2=sometimes, 3=rarely, 4=very
  rarely.
  */
  int leave_shells;
  /*
  Time between shot and shell ejected.
  */
  int leave_shell_delay;
  /*
  Play reload sound when the weapon is reloaded or not.
  */
  bool play_reload_sound;
  /*
  Whether the object should explode (produce a sObject and a sound indicated in exploSound) on worm
  collision. Note: works only if "wormCollide" is set "true" too!
  */
  bool worm_explode;
  /*
  Whether the object should explode (produce a sObject and a sound indicated in exploSound) on
  ground collision. Note: works only if "bounce" parameter equals 0.
  */
  bool expl_ground;
  /*
  Whether the object should collide with the worm and get removed.
  */
  bool worm_collide;
  /*
  Duration of firecone sprite being displayed. It is taken from the sprite sheet, is not animated
  and always uses same sprites (9-15, depending on crosshair position).
  */
  int fire_cone;
  /*
  Whether the object should collide with other objects. If yes, they will bounce off themselves but
  none of them will be destroyed. If set to false, they pass through each other. Note: this property
  doesn't work if wObject "blowAway" parameter equals 0! Note: this property works also if
  "detectDistance" parameter of the wObject is < 0.
  */
  bool collide_with_objects;
  /*
  Whether the object is affected by explosions' push force (on collision with sObject).
  Note: works only if the colliding sObject has got detectRange > 0 and damage > 0 and blowAway ≠ 0!
  */
  bool affect_by_explosions;
  /*
  Speed multiply on hitting rock/dirt obstacle or the edge of the map. After every bounce, the
  projectile will get this percentage of its original speed. Note: if you set it to -100, then the
  bullet will pass through rock / dirt (this "wallhack" feature works only for wObject, it doesn't
  work for nObject).
  */
  int bounce;
  /*
  Time to explode in frames (the duration time of the object before it gets removed from the map).
  When set to 0 there will be no explosion at all. Any positive value will cause creation of a
  designated sObject indicated in createOnExp parameter (if not -1) and playing explosion sound
  indicated in exploSound (if not -1). Note: the duration of the object is shortened inversely
  proportional to the "repeat" value encoded for shottype: 4 (which is 1000 for wObject 28 and 8 for
  other wObjects). Note: it is not recommended to set negative value for this property. Note: it is
  not recommended to set timeToExplo > 32767.
  */
  int time_to_explo;
  /*
  Maximum (negative) variation of time to explode in frames.
  */
  int time_to_explo_v;
  /*
  Damage inflicted on worm which was hit.
  Note: If the object has "wormCollide" property set to "false", this will be applied each frame the
  collision still occurs, leading to potentially huge damage values.
  */
  int hit_damage;
  /*
  Determines how many blood particles (nObject6) should be created on worm hit, divided by 2.
  So, if you set it to "10", then 5 blood particles will be created on worm hit (per each frame -
  which means that the amount of blood particles is affected by "wormCollide" parameter). Note: this
  property works also if wObject "hitDamage" parameter equals 0.
  */
  int blood_on_hit;
  /*
  First sprite of animation used for wObject.
  If -1, it will be a single pixel using a colour indicated in "colorBullets" parameter.
  Note: if you set -1 for this property and set "shotType": 2, then the bullet will be a single
  pixel but its colour will be changing depending on the object position (the direction the object
  is moving).
  */
  int start_frame;
  /*
  Amount of sprites to use to animate the object, starting with "startFrame".
  Note: Animation begins on random frame, so it is suitable really only for objects which have
  animation cycle which looks good regardless of what frame it starts. Think things like spinning
  grenades, mines, pulsing items, etc. Note: works properly only for shotType 0, 1 and 4 (it's
  recommended to set this parameter to 0 for shotType 2 and 3). Note: the animation cycle is
  affected by "repeat" value encoded for shottype: 4, which means that the higher the "repeat" value
  is, the delay before advancing to next frame will be lower.
  */
  int num_frames;
  /*
  Whether the animation should be looped.
  Note: loopAnim parameter is affected by "speed" parameter, which means that if you set it "true",
  then the animation cycle of the wObject will work only if the object is moving (when wObject stops
  moving, the animation cycle stops too). Note: if loopAnim is set to "false" and numFrames > 0,
  then the object will be still animated; in that case, the animation cycle of the wObject will work
  also if the object is not moving (the animation cycle keeps going even when the bullet is not
  moving) - unless the bullet stops moving on collision with ground (dirt / rock / edge of the map;
  in that case, the animation cycle always stops). Note: if loopAnim is set to "true" and numFrames:
  0 and shotType: 0, then the wObject will have randomly either the sprite indicated in startFrame
  or the next one in spritesheet (e.g. if you set startFrame 115, then the wObject will appear as
  sprite 115 or as 116; this is actually how booby trap shoots weapon packs or health packs by
  default). Note: works properly only for shotType 0, 1 and 4 (it's recommended to set this
  parameter to "false" for shotType 2 and 3).
  */
  bool loop_anim;
  /*
  Defines general type of the weapon object (wObject).
  0 - a standard object being either a colored pixel or animated sprite.
  1 - a missile-type object which uses different frames in the animation depending on its direction
  (when the bullet is turned in different angles), but only if "numFrames" paramterer is set to 0;
  in that case,"startFrame" defines the start of directional sprite range in the spritesheet (full
  sprite range is 13 sprites including the one indicated in "startFrame" parameter). In this
  shotType, wObject is not affected by addSpeed parameter. 2 - a player-controllable missile. It is
  animated like shotType: 1 (however, full sprite range is 16 sprites, including the one indicated
  in "startFrame" parameter, but only if "numFrames" paramterer is set to 0). 3 - a missile-type
  object with "drunk" behavior when "distribution" is set to non-zero value. It is animated like
  shotType: 1 (full sprite range is 13 sprites including the one indicated in "startFrame"
  parameter, but only if "numFrames" paramterer is set to 0). In this shotType, wObject is affected
  by addSpeed parameter. 4 - a type used to create very fast "laser-type" or "gauss gun" bullets,
  with hardcoded "repeat" value (for wObject 28 it has "repeat" set to 1000, and for other wObjects
  set to 8).
  */
  int shot_type;
  /*
  Color of the object. Works only if you set a pixel (startFrame: -1) for a bullet.
  Note: this parameter also affects the colour of a laser beam used for a laser weapon (wObject 28).
  */
  int color_bullets;
  /*
  Amount of nObjects to create on explosion. The wObject must actually explode, for example if
  "wormExplode" is set to false and "wormCollide" is set to true, no nObjects will be created. Note:
  NEVER set splinterAmount > 0 and splinterType to "null" for the same weapon object, otherwise the
  game will freeze!
  */
  int splinter_amount;
  /*
  Color used on nObjects (produced as splinters) when they are a single pixel (startFrame -1 or 0).
  If splinterColour is set to 0 or 1, then splinters will have a colour indicated in nObject
  "colorBullets" property. Note: if splinterColour is set to 2 or more, nObject splinter will
  actually use two colours: the one indicated in this parameter, and also the previous one.
  */
  int splinter_colour;
  /*
  Type of nObject to create when the object explodes.
  */
  int splinter_type;
  /*
  Way in which the splinter (nObject) is scattered when the wObject explodes:
  0 = all directions (like in big nuke);
  1 = direction the wObject is moving (like in mini nuke).
  */
  int splinter_scatter;
  /*
  Defines which sObject (special Object) is created as a trail. Set -1 for none.
  Note: wObject keeps creating sObjects on its trail even when it stops moving (and even when it's
  spawned or "trapped" inside rock or dirt on the map).
  */
  int obj_trail_type;
  /*
  Delay time (in frames) between creating trailing sObjects.
  Note: the delay is not referred to object's lifetime but game ticks, which means that it is
  measured in relation to the time elapsed since the game started, not since the object was created.
  */
  int obj_trail_delay;
  /*
  Defines the way in which the weapon should drop nObjects (created on its trail):
  0 - crackler-type non-directional trail (objects are dropped in all directions); in this case, the
  speed of nObject is affected by "SplinterCracklerVelDiv" parameter (section "constants"); 1 -
  larpa-type directional trail (objects are dropped in the direction the wObject is moving); in this
  case, the speed of nObject is affected by "SplinterLarpaVelDiv" parameter (section "constants").
  */
  int part_trail_type;
  /*
  Defines which nObject is created as a trail. Set -1 for none.
  Note: wObject keeps creating nObjects on its trail even when it stops moving (and even when it's
  spawned or "trapped" inside rock or dirt on the map).
  */
  int part_trail_obj;
  /*
  Delay time (in frames) between creating trailed nObjects.
  Note: the delay is not referred to object's lifetime but game ticks, which means that it is
  measured in relation to the time elapsed since the game started, not since the object was created.
  */
  int part_trail_delay;
  /*
  Defines whether the object would behave like bonuses or booby trap (i.e. whether the object would
  get removed on a collision with sObject). Note: works only if you set affectByExplosions parameter
  of the wObject to "true" AND if the colliding sObject has detectRange > 0 and damage > 0.
  */
  bool chain_explosion;

  int ComputedLoadingTime(Settings& settings) const;

  int id;
  std::string name;
  std::string id_str;
};

struct WObject : ExactObjectListBase {
  void BlowUpObject(Game& game, int cause_idx);
  void Process(Game& game);

  fixedvec pos, vel;
  // int id;
  Weapon const* type;
  int owner_idx;
  int cur_frame;
  int time_left;

  // STATS
  WormWeapon* fired_by;
  bool has_hit;
};
