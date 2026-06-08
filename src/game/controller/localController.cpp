#include "localController.hpp"

#include <chrono>
#include "../filesystem.hpp"
#include "../gfx.hpp"
#include "../keys.hpp"
#include "../reader.hpp"

#include "../ai/predictive_ai.hpp"
#include "../spectatorviewport.hpp"
#include "../viewport.hpp"
#include "../worm.hpp"

#include <cctype>
#include <memory>
#include <utility>

std::shared_ptr<WormAI> CreateAi(int controller, Worm& worm, Settings& settings) {
  if (controller == 1) return std::shared_ptr<WormAI>(new DumbLieroAI());
  if (controller == 2)
    return std::shared_ptr<WormAI>(new FollowAI(Weights(), settings.ai_parallels, worm.index == 0));

  return {};
}

LocalController::LocalController(const std::shared_ptr<Common>& common,
                                 const std::shared_ptr<Settings>& settings)
    : game(common, settings, gfx.sound_player) {
  auto worm1 = std::make_shared<Worm>();
  worm1->settings = settings->worm_settings[0];
  worm1->health = worm1->settings->health;
  worm1->index = 0;
  worm1->stats_x = 0;
  worm1->ai = CreateAi(worm1->settings->controller, *worm1, *settings);

  auto worm2 = std::make_shared<Worm>();
  worm2->settings = settings->worm_settings[1];
  worm2->health = worm2->settings->health;
  worm2->index = 1;
  worm2->stats_x = 218;
  worm2->ai = CreateAi(worm2->settings->controller, *worm2, *settings);

  game.AddViewport(new Viewport(Rect(0, 0, 158, 158), worm1->index, 504, 350));
  game.AddViewport(new Viewport(Rect(160, 0, 158 + 160, 158), worm2->index, 504, 350));

  game.AddWorm(worm1);
  game.AddWorm(worm2);

  // +68 on x to align the viewport in the middle
  game.AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
}

LocalController::~LocalController() { EndRecord(); }

void LocalController::OnKey(int key, bool key_state) {
  Worm::Control control{};
  Worm* worm = game.FindControlForKey(key, control);
  if (worm) {
    worm->clean_control_states.Set(control, key_state);

    if (control < Worm::kMaxControl) {
      // Only real controls
      worm->SetControlState(control, key_state);
    }

    if (worm->clean_control_states[WormSettings::kDig]) {
      worm->Press(Worm::kLeft);
      worm->Press(Worm::kRight);
    } else {
      if (!worm->clean_control_states[Worm::kLeft]) worm->Release(Worm::kLeft);
      if (!worm->clean_control_states[Worm::kRight]) worm->Release(Worm::kRight);
    }
  }

  if (key == kDkEscape && !going_to_menu) {
    fade_value = 31;
    going_to_menu = true;
  }
}

// Called when the controller loses focus. When not focused, it will not receive key events among
// other things.
void LocalController::Unfocus() {
  if (replay) replay->Unfocus();
  if (state == kStateWeaponSelection) ws->Unfocus();
}

// Called when the controller gets focus.
void LocalController::Focus() {
  if (state == kStateGameEnded) {
    going_to_menu = true;
    fade_value = 0;
    return;
  }
  if (state == kStateWeaponSelection) ws->Focus();
  if (replay) replay->Focus();
  if (state == kStateInitial) ChangeState(kStateWeaponSelection);
  game.Focus(gfx.play_renderer);
  // FIXME rewrite the focus function to avoid nonsense like this?
  game.Focus(gfx.single_screen_renderer);
  going_to_menu = false;
  fade_value = 0;
}

bool LocalController::Process() {
  if (state == kStateWeaponSelection) {
    // Apply key repeat for held keys during weapon selection.
    // Without SDL repeat events, we need to re-set control bits that
    // were consumed by WeaponSelection (via release/pressedOnce).
    for (std::size_t wi = 0; wi < game.worms.size(); ++wi) {
      Worm& worm = *game.worms[wi];
      for (int bit = 0; bit < 7; ++bit) {
        bool const kHeld = worm.clean_control_states[bit];
        if (kHeld) {
          if (!worm.control_states[bit]) {
            // Key is physically held but bit was consumed — apply repeat logic
            ++worm_held_frames[wi][bit];
            if (worm_held_frames[wi][bit] >= kKeyRepeatInitial &&
                (worm_held_frames[wi][bit] - kKeyRepeatInitial) % kKeyRepeatInterval == 0) {
              worm.Press(static_cast<Worm::Control>(bit));
            }
          } else {
            // Bit is set (initial press frame or just re-set) — reset counter
            worm_held_frames[wi][bit] = 0;
          }
        } else {
          worm_held_frames[wi][bit] = 0;
        }
      }
    }

    if (ws->ProcessFrame()) ChangeState(kStateGame);
  } else if (state == kStateGame || state == kStateGameEnded) {
    int const kRealFrameSkip = inverse_frame_skip ? !(cycles % frame_skip) : frame_skip;
    for (int i = 0; i < kRealFrameSkip && (state == kStateGame || state == kStateGameEnded); ++i) {
      int const kPhase = game.cycles % 2;
      for (std::size_t i = 0; i < game.worms.size(); ++i) {
        Worm& worm = *game.worms[(i + kPhase) % game.worms.size()];
        if (worm.ai.get()) {
          auto start_time = std::chrono::steady_clock::now();
          worm.ai->Process(game, worm);
          auto time = std::chrono::steady_clock::now() - start_time;
          game.stats_recorder->AiProcessTime(&worm, time);
        }
      }
      if (replay) {
        try {
          replay->RecordFrame();
        } catch (std::runtime_error& e) {
          console::WriteWarning(std::string("Error recording replay frame: ") + e.what());
          console::WriteWarning("Replay recording aborted");
          replay.reset();
        }
      }
      game.ProcessFrame();

      if (game.IsGameOver()) {
        ChangeState(kStateGameEnded);
      }
    }
  }

  // CommonController::process();

  if (going_to_menu) {
    if (fade_value > 0) {
      fade_value -= 1;
    } else {
      if (state == kStateGameEnded) {
        EndRecord();
        game.stats_recorder->Finish(game);
      }
      return false;
    }
  } else {
    if (fade_value < 33) {
      fade_value += 1;
    }
  }

  return true;
}

void LocalController::Draw(Renderer& renderer, bool use_spectator_viewports) {
  if (state == kStateWeaponSelection) {
    ws->Draw(renderer, state, use_spectator_viewports);
  } else if (state == kStateGame || state == kStateGameEnded || state == kStateInitial) {
    game.Draw(renderer, state, use_spectator_viewports);
  }
  renderer.fade_value = fade_value;
}

void LocalController::ChangeState(GameState new_state) {
  if (state == new_state) return;

  // NOTE: We prepare new state before destroying the old.
  // e.g. weapon selection is destroyed first after we successfully
  // started recording.

  // NOTE: Must do this here before starting recording!
  if (state == kStateWeaponSelection) {
    ws->Finalize();
  }

  if (new_state == kStateWeaponSelection) {
    ws = std::make_unique<WeaponSelection>(game);
  } else if (new_state == kStateGame) {
    // NOTE: This must be done before the replay recording starts below
    for (auto& i : game.worms) {
      Worm& worm = *i;
      worm.lives = game.settings->lives;
    }

    if (Settings::kExtensions && game.settings->record_replays) {
      try {
        std::time_t const kTicks = std::time(nullptr);
        std::tm* now = std::localtime(&kTicks);

        char buf[512];
        // NOLINTNEXTLINE(cert-err33-c) — buffer is generous; truncation only on a malformed locale and is non-fatal here.
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H.%M.%S", now);

        std::string player_names = " ";
        for (std::size_t i = 0; i < 2; ++i) {
          Worm const& worm = *game.worms[i];
          std::string const& name = worm.settings->name;
          int chars = 0;

          if (i > 0) player_names.push_back('-');
          for (std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars) {
            auto const kCh = static_cast<unsigned char>(name[c]);
            if (std::isalnum(kCh)) player_names.push_back(kCh);
          }
        }

        auto node = gfx.GetUserConfigNode() / "Replays" / (buf + player_names + ".lrp");

        replay = std::make_unique<ReplayWriter>(node.ToWriter());

        replay->BeginRecord(game);
      } catch (std::runtime_error& e) {
        gfx.pending_error_message = std::string("Error starting replay recording: ") + e.what();
        going_to_menu = true;
        fade_value = 0;
        return;
      }
    }

    game.StartGame();
  } else if (new_state == kStateGameEnded) {
    if (!going_to_menu) {
      fade_value = 180;
      going_to_menu = true;
    }
  }

  if (state == kStateWeaponSelection) {
    fade_value = 33;
    ws.reset();
  }

  state = new_state;
}

void LocalController::EndRecord() {
  if (replay) {
    replay.reset();
  }
}

void LocalController::SwapLevel(Level& new_level) { CurrentLevel()->Swap(new_level); }

Level* LocalController::CurrentLevel() { return &game.level; }

Game* LocalController::CurrentGame() { return &game; }

bool LocalController::Running() { return state != kStateGameEnded && state != kStateInitial; }
