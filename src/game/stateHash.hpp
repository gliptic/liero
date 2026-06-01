#pragma once

#include <cstdint>
#include "bobject.hpp"
#include "bonus.hpp"
#include "game.hpp"
#include "level.hpp"
#include "nobject.hpp"
#include "sobject.hpp"
#include "weapon.hpp"
#include "worm.hpp"

// Comprehensive hash of all simulation-relevant state.
// Used for determinism verification in tests and the desync fuzzer.
inline uint32_t hashGameState(Game& game) {
  uint32_t h = 1;

  h = h * 31 + game.rand.last;
  h = h * 31 + static_cast<uint32_t>(game.cycles);

  for (int i = 0; i < game.level.width * game.level.height; ++i) {
    h = h * 33 ^ game.level.data[i];
  }

  for (auto const& w : game.worms) {
    h = h * 31 + static_cast<uint32_t>(w->pos.x);
    h = h * 31 + static_cast<uint32_t>(w->pos.y);
    h = h * 31 + static_cast<uint32_t>(w->vel.x);
    h = h * 31 + static_cast<uint32_t>(w->vel.y);
    h = h * 31 + static_cast<uint32_t>(w->aimingAngle);
    h = h * 31 + static_cast<uint32_t>(w->health);
    h = h * 31 + static_cast<uint32_t>(w->lives);
    h = h * 31 + static_cast<uint32_t>(w->kills);
    h = h * 31 + static_cast<uint32_t>(w->timer);
    h = h * 31 + static_cast<uint32_t>(w->visible);
    h = h * 31 + w->controlStates.pack();

    for (int i = 0; i < NUM_WEAPONS; ++i) {
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].ammo);
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].delayLeft);
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].loadingLeft);
      if (w->weapons[i].type) h = h * 31 + static_cast<uint32_t>(w->weapons[i].type->id);
    }

    h = h * 31 + static_cast<uint32_t>(w->ninjarope.out);
    h = h * 31 + static_cast<uint32_t>(w->ninjarope.pos.x);
    h = h * 31 + static_cast<uint32_t>(w->ninjarope.pos.y);
  }

  {
    auto br = game.bobjects.begin();
    for (; br != game.bobjects.end(); ++br) {
      h = h * 31 + static_cast<uint32_t>(br->pos.x);
      h = h * 31 + static_cast<uint32_t>(br->pos.y);
    }
  }

  {
    auto r = game.bonuses.all();
    Bonus* b;
    while ((b = r.next())) {
      h = h * 31 + static_cast<uint32_t>(b->x);
      h = h * 31 + static_cast<uint32_t>(b->y);
      h = h * 31 + static_cast<uint32_t>(b->timer);
      h = h * 31 + static_cast<uint32_t>(b->weapon);
      h = h * 31 + static_cast<uint32_t>(b->frame);
    }
  }

  {
    auto r = game.sobjects.all();
    SObject* s;
    while ((s = r.next())) {
      h = h * 31 + static_cast<uint32_t>(s->id);
      h = h * 31 + static_cast<uint32_t>(s->curFrame);
    }
  }

  {
    auto r = game.nobjects.all();
    NObject* n;
    while ((n = r.next())) {
      h = h * 31 + static_cast<uint32_t>(n->pos.x);
      h = h * 31 + static_cast<uint32_t>(n->pos.y);
      h = h * 31 + static_cast<uint32_t>(n->vel.x);
      h = h * 31 + static_cast<uint32_t>(n->vel.y);
      h = h * 31 + static_cast<uint32_t>(n->curFrame);
      if (n->type) h = h * 31 + static_cast<uint32_t>(n->type->id);
    }
  }

  {
    auto r = game.wobjects.all();
    WObject* wo;
    while ((wo = r.next())) {
      h = h * 31 + static_cast<uint32_t>(wo->pos.x);
      h = h * 31 + static_cast<uint32_t>(wo->pos.y);
      h = h * 31 + static_cast<uint32_t>(wo->vel.x);
      h = h * 31 + static_cast<uint32_t>(wo->vel.y);
      h = h * 31 + static_cast<uint32_t>(wo->curFrame);
      h = h * 31 + static_cast<uint32_t>(wo->timeLeft);
      if (wo->type) h = h * 31 + static_cast<uint32_t>(wo->type->id);
    }
  }

  return h;
}

// Per-component hashes for diagnostic output on desync.
static constexpr size_t NumPlayers = 2;

struct ComponentHashes {
  uint32_t rng;
  uint32_t level;
  uint32_t worms[NumPlayers];
  uint32_t bobjects;
  uint32_t bonuses;
  uint32_t sobjects;
  uint32_t nobjects;
  uint32_t wobjects;
};

inline ComponentHashes hashGameComponents(Game& game) {
  ComponentHashes c{};

  c.rng = game.rand.last;

  {
    uint32_t h = 1;
    for (int i = 0; i < game.level.width * game.level.height; ++i) h = h * 33 ^ game.level.data[i];
    c.level = h;
  }

  for (size_t wi = 0; wi < game.worms.size() && wi < NumPlayers; ++wi) {
    auto const& w = game.worms[wi];
    uint32_t h = 1;
    h = h * 31 + static_cast<uint32_t>(w->pos.x);
    h = h * 31 + static_cast<uint32_t>(w->pos.y);
    h = h * 31 + static_cast<uint32_t>(w->vel.x);
    h = h * 31 + static_cast<uint32_t>(w->vel.y);
    h = h * 31 + static_cast<uint32_t>(w->health);
    h = h * 31 + static_cast<uint32_t>(w->lives);
    h = h * 31 + static_cast<uint32_t>(w->visible);
    h = h * 31 + static_cast<uint32_t>(w->timer);
    c.worms[wi] = h;
  }

  {
    uint32_t h = 1;
    auto br = game.bobjects.begin();
    for (; br != game.bobjects.end(); ++br) {
      h = h * 31 + static_cast<uint32_t>(br->pos.x);
      h = h * 31 + static_cast<uint32_t>(br->pos.y);
    }
    c.bobjects = h;
  }

  {
    uint32_t h = 1;
    auto r = game.bonuses.all();
    Bonus* b;
    while ((b = r.next())) {
      h = h * 31 + static_cast<uint32_t>(b->x);
      h = h * 31 + static_cast<uint32_t>(b->y);
      h = h * 31 + static_cast<uint32_t>(b->timer);
      h = h * 31 + static_cast<uint32_t>(b->weapon);
    }
    c.bonuses = h;
  }

  {
    uint32_t h = 1;
    auto r = game.sobjects.all();
    SObject* s;
    while ((s = r.next())) {
      h = h * 31 + static_cast<uint32_t>(s->id);
      h = h * 31 + static_cast<uint32_t>(s->curFrame);
    }
    c.sobjects = h;
  }

  {
    uint32_t h = 1;
    auto r = game.nobjects.all();
    NObject* n;
    while ((n = r.next())) {
      h = h * 31 + static_cast<uint32_t>(n->pos.x);
      h = h * 31 + static_cast<uint32_t>(n->pos.y);
    }
    c.nobjects = h;
  }

  {
    uint32_t h = 1;
    auto r = game.wobjects.all();
    WObject* wo;
    while ((wo = r.next())) {
      h = h * 31 + static_cast<uint32_t>(wo->pos.x);
      h = h * 31 + static_cast<uint32_t>(wo->pos.y);
    }
    c.wobjects = h;
  }

  return c;
}
