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

  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;

  // Called when the user selects a file. Return true to accept and pop.
  virtual bool OnSelected(FileNode* node) = 0;

  // Override to do per-frame drawing before the selector (e.g. level preview)
  virtual void DrawExtra() {}

 protected:
  FileSelector& Selector() { return *selector_; }
  std::unique_ptr<FileSelector> selector_;
  std::string title_;
  bool done_ = false;
};

// Level selector
struct LevelSelectorState : FileSelectorState {
  LevelSelectorState();
  void Enter() override;
  bool OnSelected(FileNode* node) override;
  void DrawExtra() override;

 private:
  std::shared_ptr<FileNode> randomNode_;
  FileNode* previewNode_ = nullptr;
  // Maximum columns/rows the HUD minimap can draw (Level::kHudMinimapW/H).
  // Reset on Enter(); used to clear residue from a wider previous level.
  int prev_hud_cols_ = 52;
  int prev_hud_rows_ = 36;
};

// Replay selector
struct ReplaySelectorState : FileSelectorState {
  ReplaySelectorState();
  void Enter() override;
  bool OnSelected(FileNode* node) override;
};

// Profile selector
struct ProfileSelectorState : FileSelectorState {
  ProfileSelectorState(WormSettings& ws);
  void Enter() override;
  bool OnSelected(FileNode* node) override;

 private:
  WormSettings& ws_;
};

// Options (settings file) selector
struct OptionsSelectorState : FileSelectorState {
  OptionsSelectorState();
  void Enter() override;
  bool OnSelected(FileNode* node) override;
};

// TC selector
struct TcSelectorState : FileSelectorState {
  TcSelectorState();
  void Enter() override;
  bool OnSelected(FileNode* node) override;
};
