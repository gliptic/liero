#include "rematchState.hpp"

#include "controller/controller.hpp"
#include "fileSelectorState.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "gamePlayState.hpp"
#include "gfx.hpp"
#include "inputState.hpp"
#include "keys.hpp"
#include "mixer/player.hpp"
#include "net/session.hpp"

RematchState::RematchState(Game& last_game)
    : lastGame_(last_game),
      menu_(/*centered=*/true)  // centered
{}

void RematchState::Enter() {
  Fill(gfx->play_renderer.bmp, 0);
  gfx->play_renderer.origpal = gfx->common->exepal;
  gfx->play_renderer.origpal_modern = gfx->common->modernpal;
  gfx->ClearKeys();

  prevRandomLevel_ = gfx->settings->random_level;
  prevLevelFile_ = gfx->settings->level_file;

  // Build menu items
  bool const kIsHost = gfx->net_session && gfx->net_session->IsHost();

  menu_.AddItem(MenuItem(48, 7, "LEVEL", kRmLevel));
  menu_.AddItem(MenuItem(48, 7, "READY", kRmReady));
  menu_.AddItem(MenuItem(48, 7, "DISCONNECT", kRmDisconnect));

  // Only host can select the level item
  if (!kIsHost) {
    menu_.items[0].selectable = false;
  }

  menu_.value_offset_x = 80;
  menu_.Place(120, 90);
  menu_.MoveToId(kRmReady);
}

void RematchState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

void RematchState::UpdateMenuItems() {
  if (!gfx->net_session) {
    return;
  }

  // Update level display
  auto* level_item = menu_.ItemFromId(kRmLevel);
  if (level_item) {
    level_item->has_value = true;
    level_item->value = '"' + LevelDisplayName() + '"';
  }

  // Update ready item text
  auto* ready_item = menu_.ItemFromId(kRmReady);
  if (ready_item) {
    bool const kLocalReady = gfx->net_session->LocalReady();
    ready_item->string = kLocalReady ? "CANCEL" : "READY UP";
    ready_item->color = kLocalReady ? 18 : 63;
  }
}

bool RematchState::Update() {
  if (!gfx->net_session) {
    return false;
  }

  gfx->net_session->Update();

  auto state = gfx->net_session->State();

  // Rematch game is starting — transfer controller and enter gameplay
  if (state == NetSession::kPlaying) {
    auto ctrl = gfx->net_session->ReleaseController();
    gfx->controller = std::unique_ptr<Controller>(ctrl.release());
    gfx->state_stack.ScheduleReplaceTop(std::make_unique<GamePlayState>());
    return true;
  }

  if (state == NetSession::kDisconnected || state == NetSession::kFailed) {
    gfx->net_session.reset();
    gfx->state_stack.ScheduleReplaceTop(
        std::make_unique<InfoBoxState>("PEER DISCONNECTED", 160, 100, true));
    return true;
  }

  // Check if a level selector was open and just closed
  if (levelSelectorOpen_) {
    levelSelectorOpen_ = false;

    bool const kChanged = (gfx->settings->random_level != prevRandomLevel_) ||
                          (gfx->settings->level_file != prevLevelFile_);

    if (kChanged && gfx->net_session->IsHost()) {
      gfx->net_session->SetRematchLevel(gfx->settings->random_level, gfx->settings->level_file);
    }

    prevRandomLevel_ = gfx->settings->random_level;
    prevLevelFile_ = gfx->settings->level_file;
  }

  Common const& common = *gfx->common;

  // Menu navigation
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_UP) || gfx->TestControlOnce(WormSettingsExtensions::kUp) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
    menu_.Movement(-1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_DOWN) ||
      gfx->TestControlOnce(WormSettingsExtensions::kDown) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
    menu_.Movement(1);
  }

  // Escape = disconnect
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) ||
      gfx->TestControlOnce(WormSettingsExtensions::kJump) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST)) {
    gfx->net_session->Disconnect();
    gfx->net_session.reset();
    Fill(gfx->play_renderer.bmp, 0);
    Fill(gfx->single_screen_renderer.bmp, 0);
    return false;
  }

  // Enter/Fire = activate selected item
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RETURN) || gfx->TestSdlKeyOnce(SDL_SCANCODE_KP_ENTER) ||
      gfx->TestControlOnce(WormSettingsExtensions::kFire) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
    int const kSel = menu_.SelectedId();
    switch (kSel) {
      case kRmLevel:
        // Host opens level selector
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        levelSelectorOpen_ = true;
        gfx->state_stack.Push(std::make_unique<LevelSelectorState>(), gfx);
        break;

      case kRmReady:
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        gfx->net_session->ToggleReady();
        break;

      case kRmDisconnect:
        g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
        gfx->net_session->Disconnect();
        gfx->net_session.reset();
        Fill(gfx->play_renderer.bmp, 0);
        Fill(gfx->single_screen_renderer.bmp, 0);
        return false;

      default:
        break;
    }
  }

  UpdateMenuItems();
  return true;
}

std::string RematchState::LevelDisplayName() const {
  if (gfx->settings->random_level || gfx->settings->level_file.empty()) {
    return "RANDOM";
  }
  return GetBasename(GetLeaf(gfx->settings->level_file));
}

void RematchState::Draw() {
  Common& common = *gfx->common;
  Font& font = common.font;

  Fill(gfx->play_renderer.bmp, 0);

  int const kCx = 160;
  int y = 40;

  // Title
  std::string const kTitle = "REMATCH";
  int const kTw = font.GetDims(kTitle);
  font.DrawString(gfx->play_renderer.bmp, kTitle, kCx - kTw / 2, y, 50);
  y += 14;

  // Score summary from last game
  {
    Worm const* w0 = lastGame_.WormByIdx(0);
    Worm const* w1 = lastGame_.WormByIdx(1);
    if (w0 && w1) {
      std::string const kScore = w0->settings->name + "  " + std::to_string(w0->kills) + " - " +
                                 std::to_string(w1->kills) + "  " + w1->settings->name;
      int const kSw = font.GetDims(kScore);
      font.DrawString(gfx->play_renderer.bmp, kScore, kCx - kSw / 2, y, 7);
    }
    y += 14;
  }

  // Peer ready status
  if (gfx->net_session) {
    bool const kIsHost = gfx->net_session->IsHost();
    bool const kLocalReady = gfx->net_session->LocalReady();
    bool const kRemoteReady = gfx->net_session->RemoteReady();

    std::string const kPeer = kIsHost ? "CLIENT" : "HOST";
    std::string const kYou = kIsHost ? "HOST" : "CLIENT";

    // Draw local player status
    std::string const kLocalLine = kYou + ":";
    int const kLlw = font.GetDims(kLocalLine);
    font.DrawString(gfx->play_renderer.bmp, kLocalLine, kCx - kLlw / 2 - 10, y, 7);
    // Draw indicator after text
    int const kLocalIndX = kCx - kLlw / 2 - 10 + kLlw + 4;
    if (kLocalReady) {
      font.DrawString(gfx->play_renderer.bmp, "READY", kLocalIndX, y, 63);
    } else {
      font.DrawString(gfx->play_renderer.bmp, "X", kLocalIndX, y, 18);
    }
    y += 10;

    // Draw remote player status
    std::string const kRemoteLine = kPeer + ":";
    int const kRlw = font.GetDims(kRemoteLine);
    font.DrawString(gfx->play_renderer.bmp, kRemoteLine, kCx - kRlw / 2 - 10, y, 7);
    int const kRemoteIndX = kCx - kRlw / 2 - 10 + kRlw + 4;
    if (kRemoteReady) {
      font.DrawString(gfx->play_renderer.bmp, "READY", kRemoteIndX, y, 63);
    } else {
      font.DrawString(gfx->play_renderer.bmp, "X", kRemoteIndX, y, 18);
    }
    y += 14;
  }

  // Draw menu
  menu_.Draw(common, gfx->play_renderer, /*disabled=*/false);
}
