#ifndef LIERO_WORK_QUEUE_HPP
#define LIERO_WORK_QUEUE_HPP

#include <SDL/SDL.h>
#include <vector>
#include "tl/memory.h"
#include <memory>

struct LockMutex
{
	LockMutex(SDL_mutex* m)
	: m(m)
	{
		SDL_LockMutex(m);
	}

	~LockMutex()
	{
		SDL_UnlockMutex(m);
	}

	SDL_mutex* m;
};

struct Work
{
	Work()
	: done_(false)
	{
		mutex = SDL_CreateMutex();
		stateCond = SDL_CreateCond();
	}

	~Work()
	{
		SDL_DestroyMutex(mutex);
		SDL_DestroyCond(stateCond);
	}

	bool waitDone()
	{
		LockMutex m(mutex);

		while (!done_)
			SDL_CondWait(stateCond, mutex);

		return done_;
	}

	bool done_;
	SDL_mutex* mutex;
	SDL_cond* stateCond;

	void run()
	{
		doRun();

		{
			LockMutex m(mutex);

			done_ = true;
			TL_WRITE_SYNC();

			SDL_CondSignal(stateCond);
		}
	}

	virtual void doRun() = 0;
};


struct StopWorker
{
};

struct WorkQueue
{
	WorkQueue(int threadCount)
	: queueAlive(true)
	{
		queueMutex = SDL_CreateMutex();
		queueCond = SDL_CreateCond();

		std::memset(threads, 0, sizeof(threads));
		for (int i = 0; i < threadCount; ++i)
		{
			threads[i] = SDL_CreateThread(worker, this);
		}
	}

	~WorkQueue()
	{
		{
			LockMutex m(queueMutex);

			queueAlive = false;
			SDL_CondBroadcast(queueCond);
		}

		for (int i = 0; i < 8; ++i)
		{
			int status;
			if (threads[i])
				SDL_WaitThread(threads[i], &status);
		}

		SDL_DestroyMutex(queueMutex);
		SDL_DestroyCond(queueCond);
	}

	void add(std::unique_ptr<Work> work)
	{
		LockMutex m(queueMutex);
		queue.push_back(std::move(work));
		SDL_CondSignal(queueCond);
	}

	std::unique_ptr<Work> waitForWork()
	{
		LockMutex m(queueMutex);

		while (queue.empty() && queueAlive)
			SDL_CondWait(queueCond, queueMutex);

		if (!queueAlive)
			throw StopWorker();

		auto ret = std::move(queue[0]);
		queue.erase(queue.begin());
		return ret;
	}

	static int worker(void* self)
	{
		try
		{
			WorkQueue* queue = static_cast<WorkQueue*>(self);

			while (true)
			{
				auto work = queue->waitForWork();

				work->run();
			}
		}
		catch (StopWorker&)
		{
		}

		return 0;
	}

	SDL_mutex* queueMutex;
	SDL_cond* queueCond;
	std::vector<std::unique_ptr<Work>> queue;
	bool queueAlive;

	SDL_Thread* threads[8];
};

#endif // LIERO_WORK_QUEUE_HPP
