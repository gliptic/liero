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
  Work() : done_(false) {
    mutex = SDL_CreateMutex();
    stateCond = SDL_CreateCondition();
  }

  virtual ~Work() {
    SDL_DestroyMutex(mutex);
    SDL_DestroyCondition(stateCond);
  }

  bool waitDone() {
    LockMutex m(mutex);

    while (!done_) SDL_WaitCondition(stateCond, mutex);

    return done_;
  }

  bool done_;
  SDL_Mutex* mutex;
  SDL_Condition* stateCond;

  void run() {
    doRun();

    {
      LockMutex m(mutex);

      SDL_MemoryBarrierAcquire();
      done_ = true;
      SDL_MemoryBarrierRelease();

      SDL_SignalCondition(stateCond);
    }
  }

  virtual void doRun() = 0;
};

struct StopWorker {};

struct WorkQueue {
  WorkQueue(int threadCount) : queueAlive(true) {
    queueMutex = SDL_CreateMutex();
    queueCond = SDL_CreateCondition();

    std::memset(threads, 0, sizeof(threads));
    for (int i = 0; i < threadCount; ++i) {
      std::stringstream thread_name;
      thread_name << "ai_" << i;
      threads[i] = SDL_CreateThread(worker, thread_name.str().c_str(), this);
    }
  }

  ~WorkQueue() {
    {
      LockMutex m(queueMutex);

      queueAlive = false;
      SDL_BroadcastCondition(queueCond);
    }

    for (int i = 0; i < 8; ++i) {
      int status;
      if (threads[i]) SDL_WaitThread(threads[i], &status);
    }

    SDL_DestroyMutex(queueMutex);
    SDL_DestroyCondition(queueCond);
  }

  void add(std::unique_ptr<Work> work) {
    LockMutex m(queueMutex);
    queue.push_back(std::move(work));
    SDL_SignalCondition(queueCond);
  }

  std::unique_ptr<Work> waitForWork() {
    LockMutex m(queueMutex);

    while (queue.empty() && queueAlive) SDL_WaitCondition(queueCond, queueMutex);

    if (!queueAlive) throw StopWorker();

    auto ret = std::move(queue[0]);
    queue.erase(queue.begin());
    return ret;
  }

  static int SDLCALL worker(void* self) {
    try {
      WorkQueue* queue = static_cast<WorkQueue*>(self);

      while (true) {
        auto work = queue->waitForWork();

        work->run();
      }
    } catch (StopWorker&) {
    }

    return 0;
  }

  SDL_Mutex* queueMutex;
  SDL_Condition* queueCond;
  std::vector<std::unique_ptr<Work>> queue;
  bool queueAlive;

  SDL_Thread* threads[8];
};
