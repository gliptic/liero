#pragma once

#include <string>
#include "exactObjectList.hpp"
#include "math.hpp"

struct Worm;
struct Game;
struct WormWeapon;
struct WObject;

/*
sObjects are special Objects.
They are static objects which cannot be affected by anything, however they can affect other objects
and worms. Their usual usage is to make explosions or non-movable trails. Like other types of
objects, they are indexed by their order in the array (counting started from 0).
*/
struct SObjectType {
  void create(Game& game, int x, int y, int ownedIdx, WormWeapon* firedBy, WObject* from = 0);

  /*
  which sound will be played when the object is created; set -1 for no sound.
  startSound is first index used...
  */
  int startSound;
  /*
  ...and numSounds is amount of indices used to pick the starting sound from.
  */
  int numSounds;
  /*
  Delay time (in frames) before advancing to next frame during object animation.
  */
  int animDelay;
  /*
  First sprite of animation used for the object.
  Note: unlike for wObjects and nObjects, sObjects always start at proper startFrame.
  So if you set it to -1, weird things are gonna happen on sObject animation and the game can even
  freeze.
  */
  int startFrame;
  /*
  Amount of sprites to use to animate the object, starting with "startFrame".
  */
  int numFrames;
  /*
  Maximum range of the sObject when it begins to affect the worm.
  Note: if detectRange ⩽ 0, then the worm will not receive damage from the object and also blowAway
  parameter will not work. Note: if you set detectRange > 0, bonuses (weapon/health boxes) will
  still explode on a collision with such sObject even if its damage equals 0.
  */
  int detectRange;
  /*
  Damage applied to the worm if it's in explosion range (vide detectRange parameter).
  Note: this is affected by how far the worm is from the epicenter of the explosion (the position of
  sObject) - the closer the sObject is created to the worm, the more damage the worm will get; the
  further the sObject is created to the worm, the less damage the worm will get. Note: it is very
  rare for an explosion to be in exact point where the worm is. This means, usually the damage will
  be noticeably smaller than the number indicate in this parameter. Use about 2/3 of its value as a
  rough estimate of the damage usually given to a worm.
  */
  int damage;
  /*
  Force applied on the worm as pushback. Can also be negative.
  Note: works only if detectRange > 0 and damage > 0.
  */
  int blowAway;
  /*
  Whether the sObject should create a shadow.
  */
  bool shadow;
  /*
  Duration of the screen shake effect caused by the sObject.
  */
  int shake;
  /*
  Duration of the screen flash effect caused by the sObject.
  */
  int flash;
  /*
  Which dirt mask to use on object creation. Set -1 for none.
  */
  int dirtEffect;

  int id;
  std::string idStr;
};

struct SObject : ExactObjectListBase {
  void process(Game& game);

  int x, y;
  int id;  // type
  int curFrame;
  int animDelay;
};
