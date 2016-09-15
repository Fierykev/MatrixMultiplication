#include <stdio.h>
#include <stdlib.h>

#include "scheduler.h"
#include "blockSum.h"

#define NO_STRASSEN

void split(int* P, int* C, int iB, int jB, int N) ;
void add(int* A, int* B, int N, int* C) ;
void sub(int* A, int* B, int N, int* C) ;
void join(int* C, int* P, int iB, int jB, int N) ;
void multiply(int* A, int* B, int N, int* C) ;

void multiply(int* A, int* B, int N, int* C)
{        
    if (N == 1)
        (*C) = (*A) * (*B);
    else
    {
        int A11[(N/2)*(N/2)];
        int A12[(N/2)*(N/2)];
        int A21[(N/2)*(N/2)];
        int A22[(N/2)*(N/2)];
        int B11[(N/2)*(N/2)];
        int B12[(N/2)*(N/2)];
        int B21[(N/2)*(N/2)];
        int B22[(N/2)*(N/2)];
        int M1[(N/2)*(N/2)];
        int M2[(N/2)*(N/2)];
        int M3[(N/2)*(N/2)];
        int M4[(N/2)*(N/2)];
        int M5[(N/2)*(N/2)];
        int M6[(N/2)*(N/2)];
        int M7[(N/2)*(N/2)];
        int C11[(N/2)*(N/2)];
        int C12[(N/2)*(N/2)];
        int C21[(N/2)*(N/2)];
        int C22[(N/2)*(N/2)];
        int t1[(N/2)*(N/2)];
        int t2[(N/2)*(N/2)];
        split(A, A11, 0 , 0, N/2);
        split(A, A12, 0 , N/2, N/2);
        split(A, A21, N/2, 0, N/2);
        split(A, A22, N/2, N/2, N/2);
        split(B, B11, 0 , 0, N/2);
        split(B, B12, 0 , N/2, N/2);
        split(B, B21, N/2, 0, N/2);
        split(B, B22, N/2, N/2, N/2);
        add(A11, A22, N/2, t1);
        add(B11, B22, N/2,t2);
        multiply(t1, t2, N/2, M1);
        add(A21, A22, N/2, t1);
        multiply(t1, B11, N/2,(M2));
        sub(B12,B22,N/2,t1);
        multiply(A11, t1, N/2,M3);
        sub(B21,B11,N/2,t1);
        multiply(A22, t1, N/2,M4);
        add(A11, A12, N/2,t1);
        multiply(t1, B22, N/2,M5);
        add(B11, B12, N/2,t1);
        sub(A21, A11, N/2,t2);
        multiply(t2, t1, N/2,M6);
        add(B21, B22, N/2,t1);
        sub(A12, A22, N/2,t2);
        multiply(t2, t1, N/2,M7);
        add(M1,M4,N/2,t1);
        sub(t1,M5,N/2,t2);
        add(t2, M7, N/2, C11);
        add(M3, M5, N/2, C12);
        add(M2, M4, N/2, C21);
        add(M1, M3, N/2,t2);
        sub(t2,M2,N/2,t1);
        add(t1, M6, N/2, C22);
        join(C11, C, 0 , 0, N/2);
        join(C12, C, 0 , N/2, N/2);
        join(C21, C, N/2, 0, N/2);
        join(C22, C, N/2, N/2, N/2);
    }
    /** return result **/    
}
void sub(int* A, int* B, int N, int* C)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            C[i*N+j] = A[i*N+j] - B[i*N+j];
}
void add(int* A, int* B, int N, int* C)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            C[i*N+j] = A[i*N+j] + B[i*N+j];
}
void split(int* P, int* C, int iB, int jB, int N) 
{
    for(int i1 = 0, i2 = iB; i1 < N; i1++, i2++)
        for(int j1 = 0, j2 = jB; j1 < N; j1++, j2++)
            C[i1*N+j1] = P[i2*N*2+j2];
}
void join(int* C, int* P, int iB, int jB, int N) 
{
    for(int i1 = 0, i2 = iB; i1 < N; i1++, i2++)
        for(int j1 = 0, j2 = jB; j1 < N; j1++, j2++)
            P[i2*N*2+j2] = C[i1*N+j1];
}

void multiplyCPU(void* data)
{
	SchedPass* sp = (SchedPass*)data;

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

#ifndef NO_STRASSEN
    // multiply through Strassen's algorithm
	multiply(A, B, dimension, writeBack);
#endif

#ifdef NO_STRASSEN
	// zero out write back
	for (int i = 0; i < dimension; i++)
		for (int j = 0; j < dimension; j++)
			for (int k = 0; k < dimension; k++)
				writeBack[j + i * dimension] = 0;

	// calculate the dot product on the cpu
	for (int i = 0; i < dimension; i++)
		for (int j = 0; j < dimension; j++)
			for (int k = 0; k < dimension; k++)
				writeBack[j + i * dimension] += A[i * dimension + k] * B[k * dimension + j];
#endif

	// sum up the block and delete excess data
	blockSum(data);
}
