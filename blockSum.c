#include <stdio.h>
#include <stdlib.h>

#include "scheduler.h"

void blockSum(SchedPass* sp)
{
	int* A = sp->A;
	int* B = sp->B;
	pthread_mutex_t* groupLock = sp->groupLock;
	pthread_cond_t* groupSignal = sp->groupSignal;
	int* groupProgress = sp->groupProgress;

	int dimension = sp->dimension;
	int blocksPerGroup = sp->blocksPerGroup;
	int matrixWidth = blocksPerGroup * dimension;
	int* writeBack = sp->writeBack;
	int* outputSpot = sp->outputSpot;

	// update the group data number
	// obtain a lock
	if (pthread_mutex_lock(groupLock) != 0)
	{
		printf("Cannot lock.\n");
		exit(-1);
	}

	(*groupProgress)++;

	if (*groupProgress == blocksPerGroup)
	{
		// unlock all threads on the signal
		if (pthread_cond_broadcast(groupSignal) != 0)
		{
			printf("Error in setting the signal.");
			exit(-1);
		}
	}

	// unlock the mutex
	if (pthread_mutex_unlock(groupLock) != 0)
	{
		printf("Cannot unlock.\n");
		exit(-1);
	}

	// only calculate the block sum if this thread is group leader
	if (sp->localID == 0)
	{
		// create the lock
		pthread_mutex_t waitLock;
		if (pthread_mutex_init(&(waitLock), NULL) != 0)
		{
			printf("Cannot create mutex\n");
			exit(-1);
		}
		
		// obtain a lock
		if (pthread_mutex_lock(&(waitLock)) != 0)
		{
			printf("Cannot lock.\n");
			exit(-1);
		}

		while (*groupProgress != blocksPerGroup)
			pthread_cond_wait(groupSignal, &(waitLock));
		
		// unlock the mutex
		if (pthread_mutex_unlock(&waitLock) != 0)
		{
			printf("Cannot unlock.\n");
			exit(-1);
		}

		// sum all of the blocks and write to output
		for (int i = 0; i < blocksPerGroup; i++)
		{
			for (int y = 0; y < dimension; y++)
				for (int x = 0; x < dimension; x++)
				{
					if (i == 0)
						outputSpot[y * matrixWidth + x] = writeBack[y * dimension + x];
					else
						outputSpot[y * matrixWidth + x] +=
						writeBack[i * dimension * dimension + y * dimension + x];
				}
		}

		// destroy the locks and signals
		pthread_mutex_lock(groupLock);
		pthread_mutex_destroy(groupLock);
		pthread_cond_destroy(groupSignal);

		pthread_mutex_lock(&(waitLock));
		pthread_mutex_destroy(&(waitLock));

		// remove the data from memory
		free(groupProgress);
		free(groupLock);
		free(groupSignal);
		free(A);
		free(B);
		free(writeBack);

		// delete the passing structure
		free(sp);
		
		// lessons learned from Kevin: don't finish writing a scheduler at 5:20 AM on the day the project is due
	}
}
