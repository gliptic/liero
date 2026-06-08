#include "replayController.hpp"

#include <utility>

#include <memory>

#include "../game.hpp"
#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../spectatorviewport.hpp"
#include "../viewport.hpp"

ReplayController::ReplayController(std::shared_ptr<Common> common,
                                   std::unique_ptr<io::Reader> source)
    : replay(new ReplayReader(std::move(source))), common(std::move(std::move(common))) {}

void ReplayController::OnKey(int key, bool /*key_state*/) {
  if (key == kDkEscape && !going_to_menu) {
    fade_value = 31;
    going_to_menu = true;
  }
}

// Called when the controller loses focus. When not focused, it will not receive key events among
// other things.
void ReplayController::Unfocus() {}

// Called when the controller gets focus.
void ReplayController::Focus() {
  if (state == kStateGameEnded) {
    going_to_menu = true;
    fade_value = 0;
    return;
  }
  if (state == kStateInitial) {
    try {
      game = replay->BeginPlayback(common, gfx.sound_player);
    } catch (std::runtime_error& e) {
      gfx.pending_error_message = std::string("Error starting replay playback: ") + e.what();
      going_to_menu = true;
      fade_value = 0;
      return;
    }
    replay->game = game.get();
    // Changing state first when game is available
    ChangeState(kStateGame);
  }
  game->Focus(gfx.play_renderer);
  game->Focus(gfx.single_screen_renderer);
  going_to_menu = false;
  fade_value = 0;
}

bool ReplayController::Process() {
  if (state == kStateGame || state == kStateGameEnded) {
    if (gfx.TestSdlKeyOnce(SDL_SCANCODE_R)) {
      *game = *initial_game;
      game->PostClone(*initial_game, /*complete=*/true);
      replay->reader.Seekg(initial_reader_pos);
    }

    int const kRealFrameSkip = inverse_frame_skip ? !(cycles % frame_skip) : frame_skip;
    for (int i = 0; i < kRealFrameSkip && (state == kStateGame || state == kStateGameEnded); ++i) {
      if (replay) {
        try {
          if (!replay->PlaybackFrame(*gfx.primary_renderer)) {
            // End of replay
            replay.reset();
          }
        } catch (io::StreamError& e) {
          gfx.pending_error_message = std::string("Stream error in replay: ") + e.what();
          ChangeState(kStateGameEnded);
          replay.reset();
        } catch (io::ArchiveCheckError& e) {
          gfx.pending_error_message = std::string("Archive error in replay: ") + e.what();
          ChangeState(kStateGameEnded);
          replay.reset();
        } catch (io::EndOfStream& e) {
          gfx.pending_error_message = std::string("EOF in replay: ") + e.what();
          ChangeState(kStateGameEnded);
          replay.reset();
        }
      }
      game->ProcessFrame();

      if (going_to_menu) {
        if (fade_value > 0)
          fade_value -= 1;
        else
          break;
      } else if (fade_value < 33) {
        fade_value += 1;
      }
    }

    if (game->IsGameOver()) {
      ChangeState(kStateGameEnded);
    }
  }

  CommonController::Process();

  if (going_to_menu && fade_value <= 0) {
    if (state == kStateGameEnded) {
      game->stats_recorder->Finish(*game);
    }
    return false;
  }

  if (!replay && state == kStateGame) {
    game->stats_recorder->Finish(*game);
    return false;
  }

  return true;
}

void ReplayController::Draw(Renderer& renderer, bool use_spectator_viewports) {
  if (state == kStateGame || state == kStateGameEnded) {
    game->Draw(renderer, state, use_spectator_viewports, /*is_replay=*/true);
  }
  renderer.fade_value = fade_value;
}

void ReplayController::ChangeState(GameState new_state) {
  if (state == new_state) return;

  if (new_state == kStateGame) {
    // FIXME: the viewports are changed based on the replay for some
    // reason, so we need to restore them here. Probably makes more sense
    // to not save the viewports at all. But that probably breaks save
    // format compatibility?
    game->ClearViewports();

    // for backwards compatibility reasons, this is not stored within the
    // replay. Yet.
    game->worms[0]->stats_x = 0;
    game->worms[1]->stats_x = 218;

    // spectator viewport is always full size
    // +68 on x to align the viewport in the middle
    game->AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
    if (gfx.settings->single_screen_replay) {
      // on single screen replay, use the spectator viewport for the
      // main screen as well
      // we can't use the same object, as the vector's clean function will delete them
      game->AddViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
    } else {
      game->AddViewport(new Viewport(Rect(0, 0, 158, 158), game->worms[0]->index, 504, 350));
      game->AddViewport(
          new Viewport(Rect(160, 0, 158 + 160, 158), game->worms[1]->index, 504, 350));
    }
    game->StartGame();
    initial_game = std::make_unique<Game>(*game);
    initial_game->PostClone(*game, /*complete=*/true);
    initial_reader_pos = replay->reader.Tellg();
  } else if (new_state == kStateGameEnded) {
    if (!going_to_menu) {
      fade_value = 180;
      going_to_menu = true;
    }
  }

  state = new_state;
}

void ReplayController::SwapLevel(Level& new_level) { CurrentLevel()->Swap(new_level); }

Level* ReplayController::CurrentLevel() {
  if (game.get() && replay.get()) return &game->level;
  return nullptr;
}

Game* ReplayController::CurrentGame() { return game.get(); }

bool ReplayController::Running() { return state != kStateGameEnded && state != kStateInitial; }
