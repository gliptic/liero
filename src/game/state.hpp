#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL_events.h>

struct Gfx;

// Base class for all application states (menus, game, text input, etc.)
// States are managed via a StateStack using push/pop semantics.
struct AppState {
  virtual ~AppState() = default;

  // Called once when the state is pushed onto the stack.
  virtual void Enter() {}

  // Called once when the state is popped from the stack.
  virtual void Leave() {}

  // Handle a single SDL event. Called once per event during the frame's
  // event processing phase.
  virtual void HandleEvent(SDL_Event& ev) = 0;

  // Advance the state by one frame tick.
  // Return false to signal that this state is done and should be popped.
  virtual bool Update() = 0;

  // Render this state. Called after update().
  virtual void Draw() = 0;

  // If true, the state below this one on the stack will also be drawn
  // (before this state). Useful for modal dialogs/text input overlays.
  virtual bool IsOverlay() const { return false; }

  // If true, the frame loop calls menuFlip() (palette rotation + flip).
  // If false, it calls flip() (just present + timing).
  virtual bool WantsMenuFlip() const { return true; }

  // Back-pointer to the graphics context, set by StateStack on push.
  Gfx* gfx = nullptr;
};

// Manages a stack of AppState objects. The topmost state receives events
// and is updated each frame. Overlay states cause the state below them
// to also be drawn.
class StateStack {
 public:
  // Push a new state onto the stack. Calls enter() on it.
  void Push(std::unique_ptr<AppState> state, Gfx* gfx) {
    state->gfx = gfx;
    state->Enter();
    stack_.push_back(std::move(state));
  }

  // Pop the top state. Calls leave() on it before removal.
  void Pop() {
    if (!stack_.empty()) {
      stack_.back()->Leave();
      stack_.pop_back();
    }
  }

  // Replace the top state with a new one.
  // When called from outside a state's update(), this takes effect immediately.
  // When called from inside update(), prefer scheduleReplaceTop() instead to
  // avoid destroying the calling state while still inside its update() method.
  void ReplaceTop(std::unique_ptr<AppState> state, Gfx* gfx) {
    Pop();
    Push(std::move(state), gfx);
  }

  // Schedule a replacement of the top state, applied after update() returns.
  // Safe to call from inside a state's update() — avoids destroying `this`.
  void ScheduleReplaceTop(std::unique_ptr<AppState> state) { pendingReplace_ = std::move(state); }

  // Access the topmost state, or nullptr if empty.
  AppState* Top() { return stack_.empty() ? nullptr : stack_.back().get(); }

  bool Empty() const { return stack_.empty(); }
  std::size_t Size() const { return stack_.size(); }

  // Dispatch an event to the topmost state.
  void HandleEvent(SDL_Event& ev) {
    if (auto* s = Top()) s->HandleEvent(ev);
  }

  // Update the topmost state. Returns false if the stack is empty
  // (application should quit) or if the top state signals completion.
  bool Update() {
    if (stack_.empty()) return false;

    bool const kEepRunning = stack_.back()->Update();
    Gfx* g = stack_.back()->gfx;

    // Apply deferred replacement (set via scheduleReplaceTop)
    if (pendingReplace_) {
      Pop();
      Push(std::move(pendingReplace_), g);
      return !stack_.empty();
    }

    if (!kEepRunning) {
      Pop();
      // The state completed. If there's still something on the
      // stack, keep running; otherwise signal "done".
      return !stack_.empty();
    }
    return true;
  }

  // Draw states, respecting overlay transparency.
  void Draw() {
    if (stack_.empty()) return;

    // Find the lowest state we need to draw
    std::size_t bottom = stack_.size() - 1;
    while (bottom > 0 && stack_[bottom]->IsOverlay()) --bottom;

    for (std::size_t i = bottom; i < stack_.size(); ++i) stack_[i]->Draw();
  }

  // Access the topmost state (for querying flip mode, etc.)
  AppState* Top() const { return stack_.empty() ? nullptr : stack_.back().get(); }

 private:
  std::vector<std::unique_ptr<AppState>> stack_;
  std::unique_ptr<AppState> pendingReplace_;
};
