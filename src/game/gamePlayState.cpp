#include "gamePlayState.hpp"

#include <cstdio>
#include "controller/controller.hpp"
#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "inputState.hpp"
#include "net/session.hpp"
#include "rematchState.hpp"
#include "statsState.hpp"
#include "stats_recorder.hpp"

void GamePlayState::Enter() { gfx->controller->Focus(); }

void GamePlayState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev, gfx->controller.get()); }

bool GamePlayState::Update() {
  // Poll network session if active
  if (gfx->net_session) {
    gfx->net_session->Update();
    auto state = gfx->net_session->State();
    if (state == NetSession::kDisconnected || state == NetSession::kFailed) {
      gfx->controller->MarkUnresumable();
      gfx->net_session.reset();
      gfx->state_stack.ScheduleReplaceTop(
          std::make_unique<InfoBoxState>("PEER DISCONNECTED", 320 / 2, 200 / 2, true));
      return false;
    }
    if (gfx->net_session->DesyncDetected()) {
      uint32_t const kFrame = gfx->net_session->DesyncFrame();
      char msg[64];
      // NOLINTNEXTLINE(cert-err33-c) — fixed-width message; buffer is generous for any uint32.
      snprintf(msg, sizeof(msg), "DESYNC AT FRAME %u", kFrame);
      gfx->controller->MarkUnresumable();
      gfx->net_session.reset();
      gfx->state_stack.ScheduleReplaceTop(
          std::make_unique<InfoBoxState>(msg, 320 / 2, 200 / 2, true));
      return false;
    }
  }

  if (!gfx->controller->Process()) {
    // Check for pending error message
    if (!gfx->pending_error_message.empty()) {
      std::string msg = std::move(gfx->pending_error_message);
      gfx->pending_error_message.clear();
      gfx->state_stack.ScheduleReplaceTop(
          std::make_unique<InfoBoxState>(std::move(msg), 320 / 2, 200 / 2, true));
      return false;
    }

    // Game ended — show stats if available and game actually finished.
    // statsGame() is the live game for single-player/replay and the
    // shadow Game for rollback multiplayer (live's recorder is a
    // no-op there — see RollbackController::setupShadowGame).
    Game* game = gfx->controller->StatsGame();
    if (game && game->stats_recorder) {
      auto* stats = dynamic_cast<NormalStatsRecorder*>(game->stats_recorder.get());
      if (stats && stats->game_time > 0) {
        bool const kIsMultiplayer = gfx->net_session != nullptr;

        // Transition network session to rematch state to keep connection alive
        if (kIsMultiplayer) {
          gfx->net_session->EnterRematch();
        }

        gfx->state_stack.ScheduleReplaceTop(
            std::make_unique<StatsState>(*stats, *game, kIsMultiplayer));
        return false;
      }
    }

    // Multiplayer match deliberately ended (END MATCH) but produced no
    // stats — END MATCH during weapon selection. Keep the session alive
    // and go straight to the rematch screen so the host can fix the
    // level pick; there is nothing to show on a stats screen. PeerLeft
    // and local Disconnect never reach StateGameEnded, so they fall
    // through to the session teardown below.
    auto* rollback = dynamic_cast<RollbackController*>(gfx->controller.get());
    if (gfx->net_session && rollback && rollback->State() == kStateGameEnded && game) {
      gfx->net_session->EnterRematch();
      gfx->state_stack.ScheduleReplaceTop(std::make_unique<RematchState>(*game));
      return false;
    }

    // Clear framebuffer so menu doesn't capture stale overlay content
    if (gfx->net_session) {
      gfx->net_session.reset();
      Fill(gfx->play_renderer.bmp, 0);
      Fill(gfx->single_screen_renderer.bmp, 0);
    }
    return false;
  }

  return true;
}

void GamePlayState::Draw() {
  gfx->play_renderer.Clear();
  gfx->controller->Draw(gfx->play_renderer, /*use_spectator_viewports=*/false);

  gfx->SetSpectatorLayout(gfx->controller->InWeaponSelection());
  gfx->single_screen_renderer.Clear();
  gfx->controller->Draw(gfx->single_screen_renderer, /*use_spectator_viewports=*/true);
}
