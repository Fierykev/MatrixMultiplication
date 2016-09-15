#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef enum
{
	halt = 1,
	shutdown
} ShutdownVals;

typedef enum
{
	queueFull = 1,
	lockError,
	globalWakeupError
} ErrorVals;

typedef struct ThreadPool ThreadPool;

ThreadPool* createThreadPool(int numThreads, int queueSize);

int addJob(ThreadPool* threadPool, void(*function)(void *), void* params);

int destroyThreadPool(ThreadPool* threadPool, int shutdownType);

void waitTillEmptyQueue(ThreadPool* threadPool);

#endif
