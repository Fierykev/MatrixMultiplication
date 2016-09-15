#include <stdlib.h>
#include <stdio.h>
#include "scheduler.h"
#include "threadPool.h"
#include "mMultGPU.h"
#include "mMultCPU.h"

#define MAX_CPU_THREADS 40
#define MAX_GPU_THREADS 1
#define BLOCK_SIZE 64

#define ENABLE_GPU

ThreadPool* gpuThreadPool;

Scheduler* createScheduler(int* A, int* B, int dimension)
{
	Scheduler* sched = (Scheduler*)malloc(sizeof(Scheduler));

	// set data out
	sched->A = A;
	sched->B = B;
	sched->dimension = dimension;
	sched->dataOut = (int*)malloc(sizeof(int) * dimension * dimension);

	if (sched->dataOut == NULL || sched == NULL)
	{
		printf("Not enough memory to store scheduler / output\n");
		exit(-1);
	}

	return sched;
}

void runScheduler(Scheduler* scheduler)
{
	int blocksPerSide = scheduler->dimension / BLOCK_SIZE;
	int jobs = blocksPerSide * blocksPerSide;
	int completeJobs = 0;

	int remBlocks = 0;
	int blockNum = 0;
	int rowA = 0, colA = 0, rowB = 0, colB = 0;

	int colBOffset = 0;

	int* dataA = NULL, *dataB = NULL, *dataC = NULL;
	int* groupProgress = NULL;
	pthread_mutex_t* groupLock = NULL;
	pthread_cond_t* groupSignal = NULL;

	// create the thread pool
	ThreadPool* cpuThreadPool = createThreadPool(MAX_CPU_THREADS, MAX_CPU_THREADS);

	// gpu not setup yet
#ifndef DISABLE_GPU
	if (gpuThreadPool == NULL)
	{
		gpuThreadPool = createThreadPool(MAX_GPU_THREADS, MAX_GPU_THREADS);

		// force opengl to bind to the gpu thread
		addJob(gpuThreadPool, setupGPU, NULL);
	}
#endif

	// set the scheduler passer
	SchedPass* schedPass = NULL;

	// start assigning jobs
	while (1)
	{
		if (remBlocks != 0)
		{
			// check if sched needs to be setup
			// note that no summation threads can be passed to the gpu due to a dependency loop they will create
			if (addJob(cpuThreadPool, multiplyCPU, (void*)schedPass) == queueFull)
			{
#ifndef DISABLE_GPU
				if (schedPass->localID == 0 || addJob(gpuThreadPool, multiplyGPU, (void*)schedPass) == queueFull)
#endif
				{
					// no block used
					remBlocks++;
				}
			}

			// decrement blocks
			remBlocks--;
		}
		else // need to create a new job
		{
			if (blockNum == blocksPerSide * blocksPerSide)
			{
				blockNum = 0;
				colBOffset++;

				if (colBOffset == blocksPerSide)
					break; // no more jobs
			}

			rowA = blockNum / blocksPerSide * BLOCK_SIZE;
			colA = blockNum % blocksPerSide * BLOCK_SIZE;

			rowB = blockNum % blocksPerSide * BLOCK_SIZE;
			colB = colBOffset * BLOCK_SIZE;

			// create the packed data
			if (dataA == NULL)
				dataA = (int*)malloc(sizeof(int) * BLOCK_SIZE * BLOCK_SIZE);

			if (dataB == NULL)
				dataB = (int*)malloc(sizeof(int) * BLOCK_SIZE * BLOCK_SIZE);

			if (dataA == NULL || dataB == NULL)
			{
				// stall for memory by re-running the loop
				continue;
			}

			// copy the blocks
			for (int y = 0; y < BLOCK_SIZE; y++)
				for (int x = 0; x < BLOCK_SIZE; x++)
				{
					dataA[y * BLOCK_SIZE + x] = scheduler->A[rowA * scheduler->dimension + colA + y * scheduler->dimension + x];
					dataB[y * BLOCK_SIZE + x] = scheduler->B[rowB * scheduler->dimension + colB + y * scheduler->dimension + x];
				}

			// create a place to write the data for this group
			if (colA == 0)
			{
				
				dataC = (int*)malloc(sizeof(int) * BLOCK_SIZE * BLOCK_SIZE * blocksPerSide);
				groupProgress = (int*)malloc(sizeof(int));
				*groupProgress = 0;
				groupLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
				groupSignal = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));

				// create the lock and condition
				if (pthread_mutex_init(groupLock, NULL) != 0)
				{
					printf("Cannot create mutex\n");
					exit(-1);
				}

				if (pthread_cond_init(groupSignal, NULL) != 0)
				{
					printf("Cannot create condition\n");
					exit(-1);
				}
			}

			// update the data to pass
			schedPass = (SchedPass*)malloc(sizeof(SchedPass));
			schedPass->groupID = colBOffset * blocksPerSide * blocksPerSide + blockNum / blocksPerSide;
			schedPass->localID = colA / BLOCK_SIZE;
			schedPass->groupLock = groupLock;
			schedPass->groupSignal = groupSignal;
			schedPass->groupProgress = groupProgress;
			schedPass->A = dataA;
			schedPass->B = dataB;
			schedPass->blocksPerGroup = blocksPerSide;
			schedPass->dimension = BLOCK_SIZE;
			schedPass->writeBack = &(dataC[colA * BLOCK_SIZE]);
			schedPass->outputSpot = &(scheduler->dataOut[rowA * scheduler->dimension + colB]);

			// reset data to null
			dataA = NULL;
			dataB = NULL;
			
			// update the remaining blocks
			remBlocks++;

			// update the blocknum
			blockNum++;
		}
	}
	
	// wait for the threads to exit
	destroyThreadPool(cpuThreadPool, shutdown);

#ifndef DISABLE_GPU
	// do not kill the gpu thread pool just simply wait for it to finish
	waitTillEmptyQueue(gpuThreadPool);
#endif
}

void deleteScheduler(Scheduler* scheduler)
{
	// free the output data
	free(scheduler->dataOut);

	// free the scheduler
	free(scheduler);
}

void killSchedulerGPU()
{
#ifndef DISABLE_GPU
	// force the thread to kill opengl
	addJob(gpuThreadPool, destroyGPU, NULL);

	// kill the thread that OpenGL is bound to
	destroyThreadPool(gpuThreadPool, shutdown);
#endif
}
