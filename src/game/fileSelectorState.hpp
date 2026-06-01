#pragma once

#include "gfx.hpp"
#include "menu/fileSelector.hpp"
#include "mixer/player.hpp"
#include "state.hpp"

#include <functional>
#include <memory>
#include <string>

// Generic file selector state. Draws title, selector, handles navigation.
// Subclass and override onSelected() to handle the result.
struct FileSelectorState : AppState {
  FileSelectorState(std::string title);

  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;

  // Called when the user selects a file. Return true to accept and pop.
  virtual bool onSelected(FileNode* node) = 0;

  // Override to do per-frame drawing before the selector (e.g. level preview)
  virtual void drawExtra() {}

 protected:
  FileSelector& selector() { return *selector_; }
  std::unique_ptr<FileSelector> selector_;
  std::string title_;
  bool done_ = false;
};

// Level selector
struct LevelSelectorState : FileSelectorState {
  LevelSelectorState();
  void enter() override;
  bool onSelected(FileNode* node) override;
  void drawExtra() override;

 private:
  std::shared_ptr<FileNode> randomNode_;
  FileNode* previewNode_ = nullptr;
};

// Replay selector
struct ReplaySelectorState : FileSelectorState {
  ReplaySelectorState();
  void enter() override;
  bool onSelected(FileNode* node) override;
};

// Profile selector
struct ProfileSelectorState : FileSelectorState {
  ProfileSelectorState(WormSettings& ws);
  void enter() override;
  bool onSelected(FileNode* node) override;

 private:
  WormSettings& ws_;
};

// Options (settings file) selector
struct OptionsSelectorState : FileSelectorState {
  OptionsSelectorState();
  void enter() override;
  bool onSelected(FileNode* node) override;
};

// TC selector
struct TcSelectorState : FileSelectorState {
  TcSelectorState();
  void enter() override;
  bool onSelected(FileNode* node) override;
};
