#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "mMultGPU.h"
#include "scheduler.h"

#define NUM_TESTS 10
#define MATRIX_SIZE 1280

int mA[MATRIX_SIZE][MATRIX_SIZE];
int mB[MATRIX_SIZE][MATRIX_SIZE];
int mC[MATRIX_SIZE][MATRIX_SIZE];

int main()
{
	// set a random seedn
	srand((unsigned int) time(NULL));

	// create a random matrix for now
	for (int i = 0; i < MATRIX_SIZE; i++)
		for (int j = 0; j < MATRIX_SIZE; j++)
		{
			mA[i][j] = rand();
			mB[j][i] = rand();
		}

	// create the scheduler
	Scheduler* scheduler = createScheduler((int*)mA, (int*)mB, MATRIX_SIZE);
	
    struct timespec start, finish;
    double elapsed;
	clock_gettime(CLOCK_MONOTONIC, &start);

	// run the scheduler
	for (int i = 0; i < NUM_TESTS; i++)
	{
		runScheduler(scheduler);
	}

    clock_gettime(CLOCK_MONOTONIC, &finish);
    
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

	printf("Our solution takes %f seconds.\n", elapsed / (double)NUM_TESTS);

	// kill the gpu
	killSchedulerGPU();

    // start the clock again
    clock_gettime(CLOCK_MONOTONIC, &start);

	for (int i = 0; i < NUM_TESTS; i++)
	{
		// check the output
		// calculate the dot product on the cpu
		for (int i = 0; i < MATRIX_SIZE; i++)
			for (int j = 0; j < MATRIX_SIZE; j++)
				for (int k = 0; k < MATRIX_SIZE; k++)
					mC[i][j] = k == 0 ? mA[i][k] * mB[k][j] : mC[i][j] + mA[i][k] * mB[k][j];
	}

    // get the ending time
    clock_gettime(CLOCK_MONOTONIC, &finish);
    
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

	printf("The naive implementation takes %f seconds.\n", elapsed / (double)NUM_TESTS);

	printf("Checking the two implementations yield the same values.\n");
	
	for (int y = 0; y < MATRIX_SIZE; y++)
		for (int x = 0; x < MATRIX_SIZE; x++)
			if (mC[y][x] != scheduler->dataOut[y * MATRIX_SIZE + x])
			{
				printf("Mismatch at (%i, %i): expected: %i actual: %i\n",
					x, y, mC[y][x], scheduler->dataOut[y * MATRIX_SIZE + x]);
			}

	
	// delete the scheduler
	deleteScheduler(scheduler);

	printf("Finished Comparison\n");
}
