#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// TMP
#include "scheduler.h"

#include "threadPool.h"

int poolID = 0; // for debugging

typedef struct
{
	void (*function)(void*);
	void* params;
} ThreadTask;

typedef struct ThreadPool
{
	int numThreads;
	int maxQueueSize;
	int numPending, numRunning;
	int order66; // shutdown the pool
	int front, back; // the ends of the queue 
	ThreadTask* queue;
	pthread_t* thread;
	pthread_mutex_t lock;
	pthread_cond_t notification;
	int poolID;
} ThreadPool;

static void* workerThread(void* passPool)
{
	ThreadPool* threadPool = (ThreadPool*)passPool;
	ThreadTask task;

	while (1)
	{
		// lock and wait
		pthread_mutex_lock(&(threadPool->lock));

		// obtain the lock and check for shutdown
		while ((threadPool->numPending == 0) && !threadPool->order66)
			pthread_cond_wait(&(threadPool->notification), &(threadPool->lock));
		
		// stop the loop and kill the thread
		if (threadPool->order66 == halt ||
			(threadPool->order66 == shutdown && threadPool->numPending == 0))
			break;

		// get the task
		task.function = threadPool->queue[threadPool->front].function;
		task.params = threadPool->queue[threadPool->front].params;

		// update the front of the queue
		threadPool->front = (threadPool->front + 1 == threadPool->maxQueueSize) ? 0 : threadPool->front + 1;
		threadPool->numPending--;

		// unlock the mutex
		pthread_mutex_unlock(&(threadPool->lock));

		// run the function
		(*task.function)(task.params);
	}

	// reduce the number running
	threadPool->numRunning--;

	// kill the thread
	pthread_mutex_unlock(&(threadPool->lock));
	pthread_exit(NULL);

	return 0;
}

ThreadPool* createThreadPool(int numThreads, int maxQueueSize)
{
	ThreadPool* threadPool = (ThreadPool*)malloc(sizeof(ThreadPool));

	// check malloc was correct
	if (threadPool == NULL)
	{
		printf("Out of memory\n");
		exit(-1);
	}

	// set to zero
	threadPool->numPending = 0;
	threadPool->numRunning = 0;
	threadPool->front = 0;
	threadPool->back = 0;
	threadPool->order66 = 0;

	threadPool->poolID = poolID++;

	// regular setup work for the pool
	threadPool->numThreads = numThreads;
	threadPool->maxQueueSize = maxQueueSize;
	threadPool->queue = (ThreadTask*)malloc(sizeof(ThreadTask) * maxQueueSize);

	// set asside memory for the threads and the queue
	threadPool->thread = (pthread_t*)malloc(sizeof(pthread_t) * numThreads);
	threadPool->queue = (ThreadTask*)malloc(sizeof(ThreadTask) * maxQueueSize);

	// check that malloc was not out of memory
	if (threadPool->thread == NULL || threadPool->queue == NULL)
	{
		printf("Out of memory\n");
		exit(-1);
	}

	// create the lock and condition
	if (pthread_mutex_init(&(threadPool->lock), NULL) != 0 || pthread_cond_init(&(threadPool->notification), NULL) != 0)
	{
		printf("Cannot create mutex or condition\n");
		exit(-1);
	}

	// launch the threads
	for (int i = 0; i < numThreads; i++)
	{
		if (pthread_create(&threadPool->thread[i], NULL, workerThread, (void*)threadPool) != 0)
		{
			// TODO: Kill the thread pool (Kevin is tired :()

			printf("Cannot create thread\n");
			exit(-1);
		}

		threadPool->numRunning++;
	}

	return threadPool;
}

int addJob(ThreadPool* threadPool, void(*function)(void *), void* params)
{
	int returnData = 0;

	if (threadPool == NULL || function == NULL)
	{
		printf("Null thread or function.\n");
		exit(-1);
	}

	// obtain a lock
	if (pthread_mutex_lock(&(threadPool->lock)) != 0)
	{
		printf("Cannot lock.\n");
		exit(-1);
	}

	// check if the queue is full
	if (threadPool->numPending == threadPool->maxQueueSize)
	{
		returnData = queueFull;
		goto unlock;
	}

	// add the task to the queue
	threadPool->queue[threadPool->back].function = function;
	threadPool->queue[threadPool->back].params = params;
	threadPool->back = (threadPool->back + 1 == threadPool->maxQueueSize) ? 0 : threadPool->back + 1;
	threadPool->numPending++;
	
	// update the signal
	if (pthread_cond_signal(&(threadPool->notification)) != 0)
	{
		returnData = lockError;
		goto unlock;
	}

unlock:

	// unlock the mutex
	if (pthread_mutex_unlock(&(threadPool->lock)) != 0)
		returnData = lockError;

	return returnData;
}

static void freeThreadPool(ThreadPool* threadPool)
{
	if (threadPool == NULL || threadPool->numRunning != 0)
	{
		printf("Null / Invalid thread pool\n");
		exit(-1);
	}

	// de-alocate
	if (threadPool->thread)
	{
		free(threadPool->thread);
		free(threadPool->queue);

		// lock the mutex due to allocation order
		pthread_mutex_lock(&(threadPool->lock));
		pthread_mutex_destroy(&(threadPool->lock));
		pthread_cond_destroy(&(threadPool->notification));
	}

	free(threadPool);
}

int destroyThreadPool(ThreadPool* threadPool, int shutdownType)
{
	int returnData = 0;

	// check the thread pool is valid
	if (threadPool == NULL)
	{
		printf("Null thread pool\n");
		exit(-1);
	}

	// lock the mutex
	if (pthread_mutex_lock(&(threadPool->lock)))
	{
		printf("Cannot access lock\n");
		exit(-1);
	}

	// already locked
	if (threadPool->order66)
	{
		returnData = lockError;
		goto unlock;
	}

	threadPool->order66 = shutdownType;

	// global wakeup call
	if (pthread_cond_broadcast(&(threadPool->notification)) != 0
		|| pthread_mutex_unlock(&(threadPool->lock)) != 0)
	{
		returnData = globalWakeupError;
		goto unlock;
	}

	// the circle of worker threads (praise be)
	for (int i = 0; i < threadPool->numThreads; i++)
		if (pthread_join(threadPool->thread[i], NULL) != 0)
			returnData = lockError;

unlock:

	// no error
	if (!returnData)
		freeThreadPool(threadPool);
	
	return returnData;
}

void waitTillEmptyQueue(ThreadPool* threadPool)
{
	pthread_spinlock_t spinLock;

	// create the spin lock
	if (pthread_spin_init(&spinLock, 0) != 0)
	{
		printf("Error initializing spin lock\n");

		exit(-1);
	}

	// spin till all threads are pending
	while (1)
	{
		if (pthread_spin_lock(&spinLock) != 0)
		{
			printf("Error obtaining spin lock\n");

			exit(-1);
		}
		
		// check if we should exit the lock
		if (threadPool->back == threadPool->front)
		{
			if (pthread_spin_unlock(&spinLock) != 0)
			{
				printf("Error unlocking spin lock\n");

				exit(-1);
			}

			break;
		}

		if (pthread_spin_unlock(&spinLock) != 0)
		{
			printf("Error releasing spin lock\n");

			exit(-1);
		}
	}

	// destroy the spin lock
	if (pthread_spin_destroy(&spinLock))
	{
		printf("Error destroying spin lock\n");

		exit(-1);
	}
}
