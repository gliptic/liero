#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "../common.hpp"
#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../text.hpp"
#include "../worm.hpp"
#include "menu.hpp"

using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

using FileFilter = bool (*)(string const& name, string const& ext);

struct FileNode {
  FileNode()
      : folder(true),
        id(0),
        selected_child(nullptr),
        parent(nullptr),
        filter(nullptr),
        menu(178, 28),
        filled(false) {
    menu.SetHeight(14);
  }

  FileNode(string name, string path_name, string full_path, bool folder, FileNode* parent,
           FileFilter filter = nullptr)
      : name(std::move(name)),
        full_path(std::move(full_path)),
        folder(folder),
        id(0),
        selected_child(nullptr),
        parent(parent),
        filter(filter),
        menu(178, 28),
        path_name(std::move(path_name)),
        filled(false) {
    menu.SetHeight(14);
  }

  void Fill();

  Menu& GetMenu() {
    EnsureFilled();

    if (menu.items.empty()) {
      for (auto& c : children) {
        menu.AddItem(MenuItem(c->folder ? 47 : 48, 7, c->name));
      }

      menu.MoveToFirstVisible();
    }

    return menu;
  }

  // NOLINTNEXTLINE(misc-no-recursion) — file tree walk; depth is bounded by user filesystem layout.
  FileNode* Find(string const& path) {
    if (CiCompare(full_path, path)) {
      return this;
    }
    if (!CiStartsWith(path, full_path)) {
      return nullptr;
    }
    if (!folder) {
      return nullptr;
    }

    EnsureFilled();

    for (auto& c : children) {
      auto* r = c->Find(path);
      if (r) {
        return r;
      }
    }

    return nullptr;
  }

  FsNode& GetFsNode() {
    if (!fs_node) {
      assert(parent);
      assert(parent->fs_node);
      fs_node = parent->fs_node / path_name;
    }

    return fs_node;
  }

  void EnsureFilled() {
    if (!filled && parent) {
      GetFsNode();
      Fill();
    }
  }

  string name;
  string full_path;
  bool folder;
  int id;
  FileNode* selected_child;
  FileNode* parent;
  vector<shared_ptr<FileNode> > children;
  bool (*filter)(std::string const& name, std::string const& ext);
  Menu menu;

  string path_name;
  bool filled;
  FsNode fs_node;  // Starts out invalid
};

struct ChildSort {
  using type = shared_ptr<FileNode>;

  bool operator()(type const& a, type const& b) const {
    if (a->folder == b->folder) {
      return CiLess(a->name, b->name);
    }
    return a->folder > b->folder;
  }
};

inline void FileNode::Fill() {
  assert(fs_node);
  DirectoryListing di(fs_node.Iter());

  for (auto const& name : di) {
    string const& full_path = JoinPath(this->full_path, name.name);
    auto const& ext = GetExtension(name.name);

    if (name.is_dir) {
      shared_ptr<FileNode> const kNode = std::make_shared<FileNode>(name.name, name.name, full_path,
                                                                    /*folder=*/true, this, filter);

      children.push_back(kNode);
    } else if (!filter || filter(name.name, ext)) {
      children.push_back(std::make_shared<FileNode>(GetBasename(name.name), name.name, full_path,
                                                    /*folder=*/false, this, filter));
    }
  }

  std::ranges::sort(children, ChildSort());

  filled = true;
}

struct FileSelector {
  FileSelector(Common& common, int /*x*/ = 178) : common(common) {}

  void Fill(string const& path, FileFilter filter)  // TODO: Get rid of this
  {
    Fill(FsNode(path), filter);
  }

  void Fill(FsNode node, FileFilter filter) {
    root_node.fs_node = std::move(node);
    root_node.full_path = root_node.fs_node.FullPath();
    root_node.filter = filter;
    root_node.Fill();
  }

  void Draw() {
    if (current_node && current_node->parent) {
      common.font.DrawFramedText(gfx.play_renderer.bmp, "Parent directory", 28, 20, 50);
      current_node->parent->GetMenu().Draw(common, gfx.play_renderer, /*disabled=*/true, 28,
                                           /*show_disabled_selection=*/true);
    }
    CurrentMenu().Draw(common, gfx.play_renderer, /*disabled=*/false, 178);
  }

  Menu& CurrentMenu() const { return current_node->GetMenu(); }

  void SetFolder(FileNode& fn) { current_node = &fn; }

  bool Select(string const& path) {
    FileNode* fn = root_node.Find(path);

    if (!fn) {
      return false;
    }

    FileNode* parent = fn->parent;
    if (parent) {
      FileNode* p = fn;
      while (p->parent) {
        FileNode const* ch = p;
        p = p->parent;
        p->selected_child = fn;

        for (std::size_t i = 0; i < p->children.size(); ++i) {
          if (ch == p->children[i].get()) {
            p->GetMenu().MoveTo(static_cast<int>(i));
          }
        }
      }

      if (fn->folder) {
        SetFolder(*fn);
      } else {
        SetFolder(*parent);
      }
    }

    return true;
  }

  FileNode* CurSel() const {
    if (!Menu().IsSelectionValid()) {
      return nullptr;
    }

    auto* c = current_node->children[Menu().Selection()].get();

    return c;
  }

  FileNode* Enter() {
    auto* c = CurSel();
    if (!c) {
      return nullptr;
    }

    if (c->folder) {
      current_node->selected_child = c;
      SetFolder(*c);
      return nullptr;
    }

    return c;
  }

  bool Exit() {
    if (current_node->parent == nullptr) {
      return false;
    }

    SetFolder(*current_node->parent);

    return true;
  }

  bool Process() {
    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_UP) || gfx.TestControlOnce(WormSettingsExtensions::kUp) ||
        gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

      Menu().Movement(-1);
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_DOWN) ||
        gfx.TestControlOnce(WormSettingsExtensions::kDown) ||
        gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);

      Menu().Movement(1);
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_PAGEUP)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

      Menu().MovementPage(-1);
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_PAGEDOWN)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);

      Menu().MovementPage(1);
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) ||
        gfx.TestControlOnce(WormSettingsExtensions::kJump) ||
        gfx.TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST)) {
      return false;
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_LEFT) ||
        gfx.TestControlOnce(WormSettingsExtensions::kLeft) ||
        gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
      Exit();
    }

    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_RIGHT) ||
        gfx.TestControlOnce(WormSettingsExtensions::kRight) ||
        gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
      Enter();
    }

    Menu().OnKeys(gfx.key_buf, gfx.key_buf_ptr, /*contains=*/true);

    return true;
  }

  Common& common;
  // vector<pair<FileNode*, int> > history;

  FileNode root_node;
  FileNode* current_node;

  // Menu menu;
};
