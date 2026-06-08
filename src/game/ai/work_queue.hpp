#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <sstream>
#include <vector>

struct LockMutex {
  LockMutex(SDL_Mutex* m) : m(m) { SDL_LockMutex(m); }

  ~LockMutex() { SDL_UnlockMutex(m); }

  SDL_Mutex* m;
};

struct Work {
  Work() : mutex(SDL_CreateMutex()), state_cond(SDL_CreateCondition()) {}

  virtual ~Work() {
    SDL_DestroyMutex(mutex);
    SDL_DestroyCondition(state_cond);
  }

  bool WaitDone() const {
    LockMutex const kM(mutex);

    while (!done) {
      SDL_WaitCondition(state_cond, mutex);
    }

    return done;
  }

  bool done{false};
  SDL_Mutex* mutex;
  SDL_Condition* state_cond;

  void Run() {
    DoRun();

    {
      LockMutex const kM(mutex);

      SDL_MemoryBarrierAcquire();
      done = true;
      SDL_MemoryBarrierRelease();

      SDL_SignalCondition(state_cond);
    }
  }

  virtual void DoRun() = 0;
};

struct StopWorker {};

struct WorkQueue {
  WorkQueue(int thread_count) : queue_mutex(SDL_CreateMutex()), queue_cond(SDL_CreateCondition()) {
    for (int i = 0; i < thread_count; ++i) {
      std::stringstream thread_name;
      thread_name << "ai_" << i;
      threads[i] = SDL_CreateThread(Worker, thread_name.str().c_str(), this);
    }
  }

  ~WorkQueue() {
    {
      LockMutex const kM(queue_mutex);

      queue_alive = false;
      SDL_BroadcastCondition(queue_cond);
    }

    for (auto& thread : threads) {
      int status = 0;
      if (thread) {
        SDL_WaitThread(thread, &status);
      }
    }

    SDL_DestroyMutex(queue_mutex);
    SDL_DestroyCondition(queue_cond);
  }

  void Add(std::unique_ptr<Work> work) {
    LockMutex const kM(queue_mutex);
    queue.push_back(std::move(work));
    SDL_SignalCondition(queue_cond);
  }

  std::unique_ptr<Work> WaitForWork() {
    LockMutex const kM(queue_mutex);

    while (queue.empty() && queue_alive) {
      SDL_WaitCondition(queue_cond, queue_mutex);
    }

    if (!queue_alive) {
      // NOLINTNEXTLINE(hicpp-exception-baseclass) — sentinel-only exception caught by the worker loop; std::exception inheritance would imply broader semantics it doesn't have.
      throw StopWorker();
    }

    auto ret = std::move(queue[0]);
    queue.erase(queue.begin());
    return ret;
  }

  static int SDLCALL Worker(void* self) {
    try {
      auto* queue = static_cast<WorkQueue*>(self);

      while (true) {
        auto work = queue->WaitForWork();

        work->Run();
      }
    } catch (StopWorker&) {  // NOLINT(bugprone-empty-catch) — sentinel exception used to break out
                             // of the worker loop on shutdown.
    }

    return 0;
  }

  SDL_Mutex* queue_mutex;
  SDL_Condition* queue_cond;
  std::vector<std::unique_ptr<Work>> queue;
  bool queue_alive{true};

  SDL_Thread* threads[8]{};
};
