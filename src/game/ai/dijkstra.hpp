#pragma once

#include <queue>
#include <utility>
#include <vector>

#include "math/rect.hpp"

#include "../common.hpp"
#include "../level.hpp"

struct PathNode {
  enum { kNone, kOpen, kClosed };

  PathNode() = default;

  void Reset() {
    state = kNone;
    parent = nullptr;
  }

  PathNode* parent{nullptr};
  int state{kNone};
  int g{0};
};

template <typename NodeT, typename DerivedT>
struct DijkstraState {
 private:
  DijkstraState() = default;

 public:
  using path_node_t = PathNode;

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
    while (!open_list.empty()) {
      open_list.pop();
    }
  }

  void SetOrigin(NodeT origin_init) {
    auto& derived = static_cast<DerivedT&>(*this);

    path_node_t* origin_pn = derived.GetNode(origin_init);
    origin_pn->g = 0;
    origin_pn->parent = nullptr;
    AddOpen(origin_pn);
  }

  template <typename StopPred, typename SuccessorIter>
  bool Run(StopPred stop, SuccessorIter succ_iter) {
    auto& derived = static_cast<DerivedT&>(*this);

    while (!open_list.empty() && !stop()) {
      Entry const kE = open_list.top();
      open_list.pop();

      // Skip stale entries from prior decreased_key operations.
      if (kE.node->state == path_node_t::kClosed) {
        continue;
      }
      if (kE.g != kE.node->g) {
        continue;
      }

      path_node_t* min_pn = kE.node;
      NodeT min = derived.GetNodeId(min_pn);

      min_pn->state = path_node_t::kClosed;

      succ_iter.Begin(min);

      while (succ_iter.Advance()) {
        NodeT node = succ_iter.Node();
        path_node_t* pn = derived.GetNode(node);
        if (pn->state != path_node_t::kClosed) {
          int const kG = min_pn->g + succ_iter.Cost();
          if (pn->state != path_node_t::kOpen) {
            pn->g = kG;
            pn->parent = min_pn;
            AddOpen(pn);
          } else if (kG < pn->g) {
            pn->g = kG;
            pn->parent = min_pn;
            // Push a new entry; the older one will be ignored at pop.
            open_list.push({kG, pn});
          }
        }
      }
    }

    return stop();
  }
  friend DerivedT;
};

struct LevelCell : PathNode {
  int cost;  // 1 = air, 2 = dirt, -1 = rock
};

extern int const kLevelCellCosts[8];

struct LevelCellSucc {
  explicit LevelCellSucc(int const* off) : offsets(off) {}

  void Begin(LevelCell* c_init) {
    i = -1;
    c = c_init;
  }

  bool Advance() {
    do {
      ++i;
      if (i >= 8) {
        return false;
      }
    } while (Cost() < 0);

    return true;
  }

  LevelCell* Node() const { return c + offsets[i]; }

  int Cost() const {
    LevelCell const* n = c + offsets[i];
    return kLevelCellCosts[i] * n->cost;
  }

  int const* offsets;
  LevelCell* c;
  LevelCell* target;
  int i;
};

struct DijkstraLevel : DijkstraState<LevelCell*, DijkstraLevel> {
  static int const kFactor = 4;

  int full_width{0}, full_height{0};
  int width{0}, pitch{0}, height{0};
  int cell_offsets[8]{};

  std::vector<LevelCell> cells;

  static path_node_t* GetNode(LevelCell* c) { return c; }

  // path_node_t IS the base of LevelCell; the cast is structurally safe and
  // hot enough that dynamic_cast would matter.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  static LevelCell* GetNodeId(path_node_t* c) { return static_cast<LevelCell*>(c); }

  LevelCell* Cell(int x, int y) { return &cells[(y + 1) * pitch + x + 1]; }

  IVec2 Coords(LevelCell* c) {
    int const kOffset = static_cast<int>(c - cells.data());
    int const kY = kOffset / pitch;
    int const kX = kOffset % pitch;
    return {kX - 1, kY - 1};
  }

  IVec2 CoordsLevel(LevelCell* c) { return Coords(c) * kFactor + IVec2(kFactor / 2, kFactor / 2); }

  LevelCell* CellFromPx(int x, int y) {
    x = std::min(std::max(x, 0), full_width - 1);
    y = std::min(std::max(y, 0), full_height - 1);
    x /= kFactor;
    y /= kFactor;
    return Cell(x, y);
  }

  LevelCellSucc MakeSucc() const { return LevelCellSucc(cell_offsets); }

  void Build(Level& level, Common& common) {
    full_width = level.width;
    full_height = level.height;
    width = full_width / kFactor;
    pitch = width + 2;
    height = full_height / kFactor;

    cell_offsets[0] = -pitch;
    cell_offsets[1] = pitch;
    cell_offsets[2] = -1;
    cell_offsets[3] = 1;
    cell_offsets[4] = -pitch - 1;
    cell_offsets[5] = -pitch + 1;
    cell_offsets[6] = pitch - 1;
    cell_offsets[7] = pitch + 1;

    cells.assign((width + 2) * (height + 2), LevelCell{});

    this->Reset();

    Material const* mat = common.materials;

    for (int x = 0; x < width + 2; ++x) {
      cells[x].cost = -1;
      cells[(height + 1) * pitch + x].cost = -1;
    }

    for (int y = 0; y < height + 2; ++y) {
      cells[y * pitch].cost = -1;
      cells[y * pitch + (width + 1)].cost = -1;
    }

    for (int ly = 0; ly < height; ++ly) {
      for (int lx = 0; lx < width; ++lx) {
        int cost = 1;

        for (int cy = ly * kFactor; cy < (ly + 1) * kFactor; ++cy) {
          for (int cx = lx * kFactor; cx < (lx + 1) * kFactor; ++cx) {
            if (!level.Inside(cx, cy)) {
              cost = -1;
              goto done;
            }

            Material const kM = mat[level.Pixel(cx, cy)];

            if (!kM.Background()) {
              if (kM.AnyDirt()) {
                cost = 2;
              } else {
                cost = -1;
                goto done;
              }
            }
          }
        }
      done:

        LevelCell* c = Cell(lx, ly);

        c->cost = cost;
      }
    }

    for (auto& c : cells) {
      c.Reset();
    }
  }
};
