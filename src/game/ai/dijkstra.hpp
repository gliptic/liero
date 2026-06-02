#pragma once

#include <queue>
#include <utility>
#include <vector>

#include "math/rect.hpp"

#include "../common.hpp"
#include "../level.hpp"

struct PathNode {
  enum { kNone, kOpen, kClosed };

  PathNode() : parent(0), state(kNone), g(0) {}

  void Reset() {
    state = kNone;
    parent = 0;
  }

  PathNode* parent;
  int state;
  int g;
};

template <typename NodeT, typename DerivedT>
struct DijkstraState {
  typedef PathNode path_node_t;

  // Lazy-deletion min-heap. When a node's cost is lowered, we push a new
  // entry; the stale entry remains in the heap and is skipped at pop time
  // (its cached `g` won't match the node's current `g`).
  struct Entry {
    int g;
    path_node_t* node;
  };
  struct EntryGreater {
    bool operator()(Entry const& a, Entry const& b) const { return a.g > b.g; }
  };
  std::priority_queue<Entry, std::vector<Entry>, EntryGreater> open_list;

  bool OpenEmpty() const { return open_list.empty(); }

  void AddOpen(path_node_t* node) {
    open_list.push({node->g, node});
    node->state = path_node_t::kOpen;
  }

  void Reset() {
    while (!open_list.empty()) open_list.pop();
  }

  void SetOrigin(NodeT origin_init) {
    DerivedT& derived = static_cast<DerivedT&>(*this);

    path_node_t* origin_pn = derived.GetNode(origin_init);
    origin_pn->g = 0;
    origin_pn->parent = 0;
    AddOpen(origin_pn);
  }

  template <typename StopPred, typename SuccessorIter>
  bool Run(StopPred stop, SuccessorIter succ_iter) {
    DerivedT& derived = static_cast<DerivedT&>(*this);

    while (!open_list.empty() && !stop()) {
      Entry e = open_list.top();
      open_list.pop();

      // Skip stale entries from prior decreased_key operations.
      if (e.node->state == path_node_t::kClosed) continue;
      if (e.g != e.node->g) continue;

      path_node_t* min_pn = e.node;
      NodeT min = derived.GetNodeId(min_pn);

      min_pn->state = path_node_t::kClosed;

      succ_iter.Begin(min);

      while (succ_iter.Advance()) {
        NodeT node = succ_iter.Node();
        path_node_t* pn = derived.GetNode(node);
        if (pn->state != path_node_t::kClosed) {
          int g = min_pn->g + succ_iter.Cost();
          if (pn->state != path_node_t::kOpen) {
            pn->g = g;
            pn->parent = min_pn;
            AddOpen(pn);
          } else if (g < pn->g) {
            pn->g = g;
            pn->parent = min_pn;
            // Push a new entry; the older one will be ignored at pop.
            open_list.push({g, pn});
          }
        }
      }
    }

    return stop();
  }
};

struct LevelCell : PathNode {
  int cost;  // 1 = air, 2 = dirt, -1 = rock
};

extern int const kLevelCellOffsets[8];
extern int const kLevelCellCosts[8];

struct LevelCellSucc {
  void Begin(LevelCell* c) {
    i = -1;
    this->c = c;
  }

  bool Advance() {
    do {
      ++i;
      if (i >= 8) return false;
    } while (Cost() < 0);

    return true;
  }

  LevelCell* Node() { return &c[kLevelCellOffsets[i]]; }

  int Cost() {
    LevelCell* n = c + kLevelCellOffsets[i];
    return kLevelCellCosts[i] * n->cost;
  }

  LevelCell* c;
  LevelCell* target;
  int i;
};

struct DijkstraLevel : DijkstraState<LevelCell*, DijkstraLevel> {
  static int const kFullWidth = 504;
  static int const kFullHeight = 350;

  static int const kFactor = 4;

  static int const kWidth = kFullWidth / kFactor;
  static int const kPitch = kWidth + 2;
  static int const kHeight = kFullHeight / kFactor;

  LevelCell cells[(kWidth + 2) * (kHeight + 2)];

  path_node_t* GetNode(LevelCell* c) { return c; }

  LevelCell* GetNodeId(path_node_t* c) { return static_cast<LevelCell*>(c); }

  LevelCell* Cell(int x, int y) { return &cells[(y + 1) * kPitch + x + 1]; }

  IVec2 Coords(LevelCell* c) {
    int offset = (int)(c - cells);
    int y = offset / kPitch;
    int x = offset % kPitch;
    return IVec2(x - 1, y - 1);
  }

  IVec2 CoordsLevel(LevelCell* c) { return Coords(c) * kFactor + IVec2(kFactor / 2, kFactor / 2); }

  LevelCell* CellFromPx(int x, int y) {
    x = std::min(std::max(x, 0), kFullWidth - 1);
    y = std::min(std::max(y, 0), kFullHeight - 1);
    x /= kFactor;
    y /= kFactor;
    return Cell(x, y);
  }

  void Build(Level& level, Common& common) {
    this->Reset();

    Material* mat = common.materials;

    for (int x = 0; x < kWidth + 2; ++x) {
      cells[x].cost = -1;
      cells[(kHeight + 1) * kPitch + x].cost = -1;
    }

    for (int y = 0; y < kHeight + 2; ++y) {
      cells[y * kPitch].cost = -1;
      cells[y * kPitch + (kWidth + 1)].cost = -1;
    }

    for (int ly = 0; ly < kHeight; ++ly) {
      for (int lx = 0; lx < kWidth; ++lx) {
        int cost = 1;

        for (int cy = ly * kFactor; cy < (ly + 1) * kFactor; ++cy)
          for (int cx = lx * kFactor; cx < (lx + 1) * kFactor; ++cx) {
            if (!level.Inside(cx, cy)) {
              cost = -1;
              goto done;
            }

            Material m = mat[level.Pixel(cx, cy)];

            if (!m.Background()) {
              if (m.AnyDirt()) {
                cost = 2;
              } else {
                cost = -1;
                goto done;
              }
            }
          }
      done:

        LevelCell* c = Cell(lx, ly);

        c->cost = cost;
      }
    }

    for (auto& c : cells) c.Reset();
  }
};
