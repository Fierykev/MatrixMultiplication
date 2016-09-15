#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>

typedef struct
{
	int* A;
	int* B;
	int dimension;
	int* dataOut;
} Scheduler;

typedef struct
{
	int groupID, localID;
	int* groupProgress;
	pthread_mutex_t* groupLock;
	pthread_cond_t* groupSignal;
	int* A;
	int* B;
	int blocksPerGroup;
	int dimension;
	int* writeBack;
	int* outputSpot;
} SchedPass;

Scheduler* createScheduler(int* A, int* B, int dimension);

void runScheduler(Scheduler* scheduler);

void deleteScheduler(Scheduler* scheduler);

void killSchedulerGPU();

#endif
