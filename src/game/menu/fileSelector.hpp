#pragma once

#include <string>
#include <utility>
#include <vector>
#include "../common.hpp"
#include "../text.hpp"
#include "../worm.hpp"
#include "menu.hpp"

using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

typedef bool (*FileFilter)(string const& name, string const& ext);

struct FileNode {
  FileNode()
      : folder(true), id(0), selected_child(0), parent(0), filter(0), menu(178, 28), filled(false) {
    menu.SetHeight(14);
  }

  FileNode(string const& name, string const& path_name, string const& full_path, bool folder,
           FileNode* parent, FileFilter filter = 0)
      : name(name),
        full_path(full_path),
        folder(folder),
        id(0),
        selected_child(0),
        parent(parent),
        filter(filter),
        menu(178, 28),
        path_name(path_name),
        filled(false) {
    menu.SetHeight(14);
  }

  void Fill();

  Menu& GetMenu() {
    EnsureFilled();

    if (menu.items.empty()) {
      for (auto& c : children) menu.AddItem(MenuItem(c->folder ? 47 : 48, 7, c->name));

      menu.MoveToFirstVisible();
    }

    return menu;
  }

  FileNode* Find(string const& path) {
    if (CiCompare(full_path, path)) return this;
    if (!CiStartsWith(path, full_path)) return 0;
    if (!folder) return 0;

    EnsureFilled();

    for (auto& c : children) {
      auto* r = c->Find(path);
      if (r) return r;
    }

    return 0;
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
  typedef shared_ptr<FileNode> type;

  bool operator()(type const& a, type const& b) const {
    if (a->folder == b->folder) return CiLess(a->name, b->name);
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
      shared_ptr<FileNode> node(new FileNode(name.name, name.name, full_path, true, this, filter));

      children.push_back(node);
    } else if (!filter || filter(name.name, ext)) {
      children.push_back(shared_ptr<FileNode>(
          new FileNode(GetBasename(name.name), name.name, full_path, false, this, filter)));
    }
  }

  std::sort(children.begin(), children.end(), ChildSort());

  filled = true;
}

struct FileSelector {
  FileSelector(Common& common, int x = 178) : common(common) {}

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
      current_node->parent->GetMenu().Draw(common, gfx.play_renderer, true, 28, true);
    }
    CurrentMenu().Draw(common, gfx.play_renderer, false, 178);
  }

  Menu& CurrentMenu() { return current_node->GetMenu(); }

  void SetFolder(FileNode& fn) { current_node = &fn; }

  bool Select(string const& path) {
    FileNode* fn = root_node.Find(path);

    if (!fn) return false;

    FileNode* parent = fn->parent;
    if (parent) {
      FileNode* p = fn;
      while (p->parent) {
        FileNode* ch = p;
        p = p->parent;
        p->selected_child = fn;

        for (std::size_t i = 0; i < p->children.size(); ++i) {
          if (ch == p->children[i].get()) p->GetMenu().MoveTo((int)i);
        }
      }

      if (fn->folder)
        SetFolder(*fn);
      else
        SetFolder(*parent);
    }

    return true;
  }

  FileNode* CurSel() {
    if (!Menu().IsSelectionValid()) return 0;

    auto* c = current_node->children[Menu().Selection()].get();

    return c;
  }

  FileNode* Enter() {
    auto* c = CurSel();
    if (!c) {
      return 0;
    }

    if (c->folder) {
      current_node->selected_child = c;
      SetFolder(*c);
      return 0;
    }

    return c;
  }

  bool Exit() {
    if (current_node->parent == 0) return false;

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

    Menu().OnKeys(gfx.key_buf, gfx.key_buf_ptr, true);

    return true;
  }

  Common& common;
  // vector<pair<FileNode*, int> > history;

  FileNode root_node;
  FileNode* current_node;

  // Menu menu;
};
