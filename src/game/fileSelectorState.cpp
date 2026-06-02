#include "fileSelectorState.hpp"

#include "common.hpp"
#include "controller/controller.hpp"
#include "controller/replayController.hpp"
#include "gfx.hpp"
#include "keys.hpp"
#include "level.hpp"
#include "menu/mainMenu.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"
#include "text.hpp"

using std::shared_ptr;
using std::string;

// --- FileSelectorState base ---

FileSelectorState::FileSelectorState(std::string title) : title_(std::move(title)) {}

void FileSelectorState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool FileSelectorState::Update() {
  if (done_) return false;

  if (!selector_->Process()) {
    done_ = true;
    return false;
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RETURN) || gfx->TestSdlKeyOnce(SDL_SCANCODE_KP_ENTER) ||
      gfx->TestControlOnce(WormSettingsExtensions::kFire) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
    g_sound_player->Play(gfx->common->sound_hook[SoundMenuSelect]);

    auto* sel = selector_->Enter();
    if (sel) {
      if (OnSelected(sel)) {
        done_ = true;
        return false;
      }
    }
  }

  return true;
}

void FileSelectorState::Draw() {
  gfx->play_renderer.bmp.Copy(gfx->frozen_screen);

  if (!title_.empty()) {
    string display_title = title_;
    if (!selector_->current_node->full_path.empty()) {
      display_title += ' ';
      display_title += selector_->current_node->full_path;
    }

    gfx->common->font.DrawFramedText(gfx->play_renderer.bmp, display_title, 178, 20, 50);
  }

  DrawExtra();
  selector_->Draw();
}

// --- LevelSelectorState ---

LevelSelectorState::LevelSelectorState() : FileSelectorState("") {}

void LevelSelectorState::Enter() {
  Common& common = *gfx->common;
  // title_ left empty — LevelSelectorState draws its own title in drawExtra()
  selector_ = std::make_unique<FileSelector>(common);

  randomNode_ = std::make_shared<FileNode>(LS(Random), "", "", false, &selector_->root_node);

  selector_->Fill(gfx->GetConfigNode(),
                  [](string const& name, string const& ext) { return CiCompare(ext, "LEV"); });

  randomNode_->id = 1;
  selector_->root_node.children.insert(selector_->root_node.children.begin(), randomNode_);
  selector_->SetFolder(selector_->root_node);
  selector_->Select(gfx->settings->level_file);

  previewNode_ = nullptr;
}

bool LevelSelectorState::OnSelected(FileNode* node) {
  if (node == randomNode_.get()) {
    gfx->settings->random_level = true;
    gfx->settings->level_file.clear();
  } else {
    gfx->settings->random_level = false;
    gfx->settings->level_file = node->full_path;
  }
  return true;
}

void LevelSelectorState::DrawExtra() {
  Common& common = *gfx->common;
  FileNode* sel = selector_->CurSel();

  if (previewNode_ != sel && sel && sel != randomNode_.get() && !sel->folder) {
    Level level(common);

    try {
      auto r_ptr = sel->GetFsNode().ToReader();
      io::Reader& r = *r_ptr;
      if (level.load(common, *gfx->settings, r)) {
        int center_x = gfx->single_screen_renderer.render_res_x / 2;

        level.DrawMiniature(gfx->frozen_screen, 134, 162, 10);
        level.DrawMiniature(gfx->frozen_spectator_screen, center_x - 126,
                            gfx->single_screen_renderer.render_res_y - 208, 2);
      }
    } catch (std::runtime_error&) {
      // Ignore
    }

    previewNode_ = sel;
  }

  // Draw title with width for rounded box (level selector uses different title rendering)
  string title = LS(SelLevel);
  if (!selector_->current_node->full_path.empty()) {
    title += ' ';
    title += selector_->current_node->full_path;
  }

  int wid = common.font.GetDims(title);
  DrawRoundedBox(gfx->play_renderer.bmp, 178, 20, 0, 7, wid);
  common.font.DrawString(gfx->play_renderer.bmp, title, 180, 21, 50);
}

// --- ReplaySelectorState ---

ReplaySelectorState::ReplaySelectorState() : FileSelectorState("Select replay:") {}

void ReplaySelectorState::Enter() {
  selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

  selector_->Fill(gfx->GetConfigNode(),
                  [](string const& name, string const& ext) { return CiCompare(ext, "LRP"); });

  selector_->SetFolder(selector_->root_node);
  if (gfx->prev_selected_replay_path.empty() ||
      !selector_->Select(gfx->prev_selected_replay_path)) {
    selector_->Select(JoinPath(gfx->GetConfigNode().FullPath(), "Replays"));
  }
}

bool ReplaySelectorState::OnSelected(FileNode* node) {
  gfx->prev_selected_replay_path = node->full_path;

  // Reset controller before opening the replay, since we may be recording it
  gfx->controller.reset();
  gfx->controller.reset(new ReplayController(gfx->common, node->GetFsNode().ToReader()));

  gfx->pending_menu_selection = MainMenu::kMaReplay;
  return true;
}

// --- ProfileSelectorState ---

ProfileSelectorState::ProfileSelectorState(WormSettings& ws)
    : FileSelectorState("Select profile:"), ws_(ws) {}

void ProfileSelectorState::Enter() {
  selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

  selector_->Fill(gfx->GetConfigNode(),
                  [](string const& name, string const& ext) { return CiCompare(ext, "TOML"); });

  selector_->SetFolder(selector_->root_node);
  selector_->Select(JoinPath(gfx->GetConfigNode().FullPath(), "Profiles"));
}

bool ProfileSelectorState::OnSelected(FileNode* node) {
  ws_.LoadProfile(node->GetFsNode());
  // Refresh the player menu so it reflects the newly loaded profile
  gfx->player_menu.UpdateItems(*gfx->common);
  return true;
}

// --- OptionsSelectorState ---

OptionsSelectorState::OptionsSelectorState() : FileSelectorState("Select options:") {}

void OptionsSelectorState::Enter() {
  selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

  selector_->Fill(gfx->GetConfigNode(),
                  [](string const& name, string const& ext) { return CiCompare(ext, "CFG"); });

  selector_->SetFolder(selector_->root_node);
  selector_->Select(JoinPath(gfx->GetConfigNode().FullPath(), "Setups"));
}

bool OptionsSelectorState::OnSelected(FileNode* node) {
  gfx->LoadSettings(node->GetFsNode());
  gfx->settings_menu.UpdateItems(*gfx->common);
  return true;
}

// --- TcSelectorState ---

TcSelectorState::TcSelectorState() : FileSelectorState("Select TC:") {}

void TcSelectorState::Enter() {
  selector_ = std::make_unique<FileSelector>(*gfx->common, 28);

  selector_->Fill(gfx->GetConfigNode() / "TC", 0);
  selector_->SetFolder(selector_->root_node);

  auto end = std::remove_if(selector_->root_node.children.begin(),
                            selector_->root_node.children.end(), [](shared_ptr<FileNode> const& n) {
                              auto tc = n->GetFsNode() / "tc.cfg";
                              return !tc.Exists();
                            });

  selector_->root_node.children.erase(end, selector_->root_node.children.end());

  for (auto& c : selector_->root_node.children) {
    c->folder = false;
  }
}

bool TcSelectorState::OnSelected(FileNode* node) {
  std::unique_ptr<Common> new_common(new Common());
  new_common->load(node->GetFsNode());
  gfx->settings->tc = node->name;
  gfx->common.reset(new_common.release());
  if (auto* dp = dynamic_cast<DefaultSoundPlayer*>(gfx->sound_player.get()))
    dp->SetCommon(*gfx->common);
  gfx->pending_menu_selection = MainMenu::kMaTc;
  return true;
}
