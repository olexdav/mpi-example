#include "stdafx.h"
#include "conio.h"
#include <cstdlib>
#include <ctime>
#include "mpi.h"

#pragma warning(disable:4996)

//#include <iostream>
//using namespace std;

int ProcNum; // Number of available processes
int ProcRank; // Rank of current process

// Function for simple definition of matrix and vector elements
void DummyDataInitialization(double* pMatrix, double* pVector, int Size) {
	int i, j; // Loop variables
	for (i = 0; i<Size; i++) {
		pVector[i] = 1;
		for (j = 0; j<Size; j++)
			pMatrix[i*Size + j] = i;
	}
}

// Function for random initialization of objects’ elements
void RandomDataInitialization(double* pMatrix, double* pVector, int Size) {
	int i, j; // Loop variables
	srand(unsigned(clock()));
	for (i = 0; i<Size; i++) {
		pVector[i] = rand() / double(1000);
		for (j = 0; j<Size; j++)
			pMatrix[i*Size + j] = rand() / double(1000);
	}
}

// Function for memory allocation and definition of objects’ elements
void ProcessInitialization(double* &pMatrix, double* &pVector,
	double* &pResult, double* &pProcRows, double* &pProcResult,
	int &Size, int &RowNum) {
	//MPI_Barrier(MPI_COMM_WORLD);
	if (ProcRank == 0) {
		do {
			printf("\nEnter size of the initial objects: ");
			scanf("%d", &Size);
			if (Size < ProcNum) {
				printf("Size of the objects must be greater than "
					"number of processes! \n ");
			}
			if (Size%ProcNum != 0) {
				printf("Size of objects must be divisible by "
					"number of processes! \n");
			}
		} while ((Size < ProcNum) || (Size%ProcNum != 0));
	}
	//MPI_Barrier(MPI_COMM_WORLD);
	MPI_Bcast(&Size, 1, MPI_INT, 0, MPI_COMM_WORLD);

	// Determine the number of matrix rows stored on each process
	RowNum = Size / ProcNum;

	// Memory allocation
	pVector = new double[Size];
	pResult = new double[Size];
	pProcRows = new double[RowNum*Size];
	pProcResult = new double[RowNum];

	// Obtain the values of initial objects’ elements
	if (ProcRank == 0) {
		// Initial matrix exists only on the pivot process
		pMatrix = new double[Size*Size];
		// Values of elements are defined only on the pivot process
		//DummyDataInitialization(pMatrix, pVector, Size);
		RandomDataInitialization(pMatrix, pVector, Size);
	}
}

// Function for computational process termination
void ProcessTermination(double* pMatrix, double* pVector, double* pResult,
	double* pProcRows, double* pProcResult) {
	if (ProcRank == 0)
		delete[] pMatrix;
	delete[] pVector;
	delete[] pResult;
	delete[] pProcRows;
	delete[] pProcResult;
}

// Function for formatted matrix output
void PrintMatrix(double* pMatrix, int RowCount, int ColCount) {
	int i, j; // Loop variables
	for (i = 0; i<RowCount; i++) {
		for (j = 0; j<ColCount; j++)
			printf("%7.4f ", pMatrix[i*ColCount + j]);
		printf("\n");
	}
}
// Function for formatted vector output
void PrintVector(double* pVector, int Size) {
	int i;
	for (i = 0; i<Size; i++)
		printf("%7.4f ", pVector[i]);
	printf("\n");
}

// Function for matrix-vector multiplication
void SerialResultCalculation(double* pMatrix, double* pVector, double* pResult,
	int Size) {
	int i, j; // Loop variables
	for (i = 0; i<Size; i++) {
		pResult[i] = 0;
		for (j = 0; j<Size; j++)
			pResult[i] += pMatrix[i*Size + j] * pVector[j];
	}
}

// Function for distribution of the initial objects between the processes
void DataDistribution(double* pMatrix, double* pProcRows, double* pVector,
	int Size, int RowNum) {
		MPI_Bcast(pVector, Size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		MPI_Scatter(pMatrix, RowNum*Size, MPI_DOUBLE, pProcRows, RowNum*Size,
			MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

void TestDistribution(double* pMatrix, double* pVector, double* pProcRows,
	int Size, int RowNum) {
	if (ProcRank == 0) {
		printf("Initial Matrix: \n");
		PrintMatrix(pMatrix, Size, Size);
		printf("Initial Vector: \n");
		PrintVector(pVector, Size);
	}
	MPI_Barrier(MPI_COMM_WORLD);
	for (int i = 0; i<ProcNum; i++) {
		if (ProcRank == i) {
			printf("\nProcRank = %d \n", ProcRank);
			printf(" Matrix Stripe:\n");
			PrintMatrix(pProcRows, RowNum, Size);
			printf(" Vector: \n");
			PrintVector(pVector, Size);
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}
}

// Process rows and vector multiplication
void ParallelResultCalculation(double* pProcRows, double* pVector,
	double* pProcResult, int Size, int RowNum) {
	int i, j;
	for (i = 0; i<RowNum; i++) {
		pProcResult[i] = 0;
		for (j = 0; j<Size; j++) {
			pProcResult[i] += pProcRows[i*Size + j] * pVector[j];
		}
	}
}

// Fuction for testing the results of multiplication of matrix stripe
// by a vector
void TestPartialResults(double* pProcResult, int RowNum) {
	int i; // Loop variable
	for (i = 0; i<ProcNum; i++) {
		if (ProcRank == i) {
			printf("ProcRank = %d \n", ProcRank);
			printf("Part of result vector: \n");
			PrintVector(pProcResult, RowNum);
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}
}

// Result vector replication
void ResultReplication(double* pProcResult, double* pResult, int Size,
	int RowNum) {
	MPI_Allgather(pProcResult, RowNum, MPI_DOUBLE, pResult, RowNum,
		MPI_DOUBLE, MPI_COMM_WORLD);
}

// Testing the result of parallel matrix-vector multiplication
void TestResult(double* pMatrix, double* pVector, double* pResult,
	int Size) {
	// Buffer for storing the result of serial matrix-vector multiplication
	double* pSerialResult;
	int equal = 0; // Flag, that shows wheather the vectors are identical
	int i; // Loop variable
	if (ProcRank == 0) {
		pSerialResult = new double[Size];
		SerialResultCalculation(pMatrix, pVector, pSerialResult, Size);
		for (i = 0; i<Size; i++) {
			if (pResult[i] != pSerialResult[i])
				equal = 1;
		}
		if (equal == 1)
			printf("The results of serial and parallel algorithms "
				"are NOT identical. Check your code.");
		else
			printf("The results of serial and parallel algorithms are "
				"identical.");
		delete[] pSerialResult;
	}
}

int main(int argc, char* argv[]) {
	double* pMatrix;     // The first argument - initial matrix
	double* pVector;     // The second argument - initial vector
	double* pResult;     // Result vector for matrix-vector multiplication
	int Size;            // Sizes of initial matrix and vector
	double* pProcRows;   // Stripe of the matrix on current process
	double* pProcResult; // Block of result vector on current process
	int RowNum;          // Number of rows in matrix stripe
	double start, finish, duration;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &ProcNum);
	MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);

	if (ProcRank == 0)
		printf("Parallel matrix - vector multiplication program\n");

	// Memory allocation and data initialization
	ProcessInitialization(pMatrix, pVector, pResult, pProcRows, pProcResult,
		Size, RowNum);

	// Distributing the initial objects between the processes
	DataDistribution(pMatrix, pProcRows, pVector, Size, RowNum);
	//TestDistribution(pMatrix, pVector, pProcRows, Size, RowNum);

	// Process rows and vector multiplication
	ParallelResultCalculation(pProcRows, pVector, pProcResult, Size, RowNum);
	//TestPartialResults(pProcResult, RowNum);

	// Result replication
	ResultReplication(pProcResult, pResult, Size, RowNum);
	TestResult(pMatrix, pVector, pResult, Size);

	// Process termination
	ProcessTermination(pMatrix, pVector, pResult, pProcRows, pProcResult);
	MPI_Finalize();
}