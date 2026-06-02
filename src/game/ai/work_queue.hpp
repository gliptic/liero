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
  Work() : done(false) {
    mutex = SDL_CreateMutex();
    state_cond = SDL_CreateCondition();
  }

  virtual ~Work() {
    SDL_DestroyMutex(mutex);
    SDL_DestroyCondition(state_cond);
  }

  bool WaitDone() {
    LockMutex m(mutex);

    while (!done) SDL_WaitCondition(state_cond, mutex);

    return done;
  }

  bool done;
  SDL_Mutex* mutex;
  SDL_Condition* state_cond;

  void Run() {
    DoRun();

    {
      LockMutex m(mutex);

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
  WorkQueue(int thread_count) : queue_alive(true) {
    queue_mutex = SDL_CreateMutex();
    queue_cond = SDL_CreateCondition();

    std::memset(threads, 0, sizeof(threads));
    for (int i = 0; i < thread_count; ++i) {
      std::stringstream thread_name;
      thread_name << "ai_" << i;
      threads[i] = SDL_CreateThread(Worker, thread_name.str().c_str(), this);
    }
  }

  ~WorkQueue() {
    {
      LockMutex m(queue_mutex);

      queue_alive = false;
      SDL_BroadcastCondition(queue_cond);
    }

    for (int i = 0; i < 8; ++i) {
      int status;
      if (threads[i]) SDL_WaitThread(threads[i], &status);
    }

    SDL_DestroyMutex(queue_mutex);
    SDL_DestroyCondition(queue_cond);
  }

  void Add(std::unique_ptr<Work> work) {
    LockMutex m(queue_mutex);
    queue.push_back(std::move(work));
    SDL_SignalCondition(queue_cond);
  }

  std::unique_ptr<Work> WaitForWork() {
    LockMutex m(queue_mutex);

    while (queue.empty() && queue_alive) SDL_WaitCondition(queue_cond, queue_mutex);

    if (!queue_alive) throw StopWorker();

    auto ret = std::move(queue[0]);
    queue.erase(queue.begin());
    return ret;
  }

  static int SDLCALL Worker(void* self) {
    try {
      WorkQueue* queue = static_cast<WorkQueue*>(self);

      while (true) {
        auto work = queue->WaitForWork();

        work->Run();
      }
    } catch (StopWorker&) {
    }

    return 0;
  }

  SDL_Mutex* queue_mutex;
  SDL_Condition* queue_cond;
  std::vector<std::unique_ptr<Work>> queue;
  bool queue_alive;

  SDL_Thread* threads[8];
};
