// PreyPredatorMPI.cpp : main project file.
// Defines the entry point for the console application.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <iostream>
#include <omp.h>
#include <time.h> 
#include <array>
#include <mpi.h>
using namespace std;

//speed selected will display every nth generation (1,10 or 100)
#define FAST 100
#define MEDIUM 10
#define SLOW 1

//array dimensions
#define HEIGHT 1024
#define WIDTH 2048  //does not have to be a multiple of the number of processes, however processes have to be even

//total number of steps to be done
#define NUMBER_OF_STEPS 500

//global map with 2 extra rows and 2 extra columns to deal with boundaries
int oldMap[HEIGHT + 2][WIDTH + 2] = { 0 };

int newMap[HEIGHT + 2][WIDTH + 2];

//processes and threads say hello when created
bool polite = true;

//only set to true on small oceans
bool display = false;

//ages all the fish and sharks, kills the ones that should die, and spawns the ones that live
//all changes are stored in newMap and then copied into oldMap
void update(int myID, int nprocs);

//displays the whole grid to the console
void print();
//returns the number of fish and sharks as a pair (fish, shark) in the whole ocean
pair<int, int> countOceanMembers(int myID, int nprocs, int myThreadID);

//obsolete method
pair<int, int> analyze();
//returns the number of fish and sharks as a pair (fish, shark) for the current process
pair<int, int> analyzeCurrentProcess(int myID, int nprocs);
//returns the number of neighboring fish and sharks and specifies how many are adults  of a given cell
//where i and j specify the location of that cell
//the returned format will be in an array of four numbers:
//number of all neighboring fish, number of adult neighboring fish, number of all neighboring sharks, number of adult neighboring sharks)
//array<int, 4>  neighborCount(int i, int j);
void evaluate(int value, int &fish, int &adultFish, int &sharks, int &adultSharks);

int main(int argc, char *argv[])
{
	/* initialize MPI */
	//nprocs will hold the number of processes there are, and myID will hold this process's current ID
	int nprocs, myID;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myID);
	MPI_Status status;

	if(polite)
		cout << "hello i am process: " << myID+1 << " out of " << nprocs << endl;
	fflush(stdout);

	clock_t t1, t2;
	t1 = clock();
	//setting the speed
	int speed = FAST;


	//only the first process needs to initialize the ocean/world
	int i, j = 0;
	float randFloat;
	if (myID == 0)
	{
		//initializing the array with 50% fish, 25% sharks, and 25% empty cells
		//note that the borders (2 rows and 2 columns) are left empty as they will be overwritten next
		for (i = 1; i <= HEIGHT; i++) {
			for (j = 1; j <= WIDTH; j++) {
				randFloat = rand() / ((float)RAND_MAX + 1);
				if (randFloat < 0.25) {
					//25% are empty
					oldMap[i][j] = 0;
				}
				else if (randFloat >= 0.25 && randFloat < 0.5) {
					//25% are sharks
					oldMap[i][j] = -1;
				}
				else {
					//50% are fish
					oldMap[i][j] = 1;
				}
				//******optional for testing
				//oldMap[i][j] = (i*WIDTH + j)%10;
				//end of optional code

			}
		}
	}
	//now the first process has to send the 2D array to the other processes
	if (myID == 0) {
		//send
		//send the portion of the map to the correct process by dividing the grid evenly along the width
		//this for loop goes from 1 to number of processes-1 and sends data to each process
		for (int destinationProcess = 1; destinationProcess < nprocs; destinationProcess++) {
			//going through all the i's for every process
			for (i = 1; i <= HEIGHT; i++) {
				//each process should only take 1/nprocs of the width
				for (int k = ((destinationProcess*WIDTH / nprocs) + 1); k <= WIDTH*(destinationProcess + 1) / nprocs; k++) {
					MPI_Send(&oldMap[i][k], 1, MPI_INT, destinationProcess, k, MPI_COMM_WORLD); //the tag is k
				}
			}
		}
	}
	else {
		//receive 
		for (i = 1; i <= HEIGHT; i++) {
			//each process should only take 1/nprocs of the width
			for (int k = ((myID*WIDTH / nprocs) + 1); k <= WIDTH*(myID + 1) / nprocs; k++) {
				MPI_Recv(&oldMap[i][k], 1, MPI_INT, 0, k, MPI_COMM_WORLD, &status); //the tag is k
			}
		}

	}

	//code snippet to test that changing the data on other processes is working properly
	/*	if (myID!=0) {
	for (i = 1; i <= HEIGHT; i++) {
	for (j = 1; j <= WIDTH; j++) {
	oldMap[i][j]--;
	if (oldMap[i][j] < 0) { oldMap[i][j] = 9; }
	}
	}
	}
	*/

	//to repeat the simulation NUMBER_OF_STEPS of times
	for (int n = 0; n <= NUMBER_OF_STEPS; n++) {

		//firstly each process needs to send its last column to the process on its right(modulo nprocs)
		//and send its first column to the process on its left (with process 0 sending to process nprocs-1)
		//simultaneously each process needs to receive its neighboring columns
		for (i = 1; i <= HEIGHT; i++) {
			int right, left;
			right = (myID + 1) % nprocs;
			left = (myID + nprocs - 1) % nprocs; //adding nprocs to eliminate the chance of getting negative process ID
			if (myID % 2 == 0) { //even numbered process
								 //sending the last column to the process on the right (next ID)
				MPI_Send(&oldMap[i][WIDTH*(right) / nprocs], 1, MPI_INT, right, 021, MPI_COMM_WORLD); //the tag says 0 to 1, ie even to next odd
																									  //receiving from the previous process and placing it before the leftmost column
				MPI_Recv(&oldMap[i][(myID*WIDTH / nprocs)], 1, MPI_INT, left, 122, MPI_COMM_WORLD, &status); //the tag says zero to 1, ie odd to next even

																											 //sending the first column to the process on the left (myID-1)
				MPI_Send(&oldMap[i][(WIDTH*myID / nprocs) + 1], 1, MPI_INT, left, 221, MPI_COMM_WORLD); //the tag says two to 1

																										//receiving from the process on the right
				MPI_Recv(&oldMap[i][((myID + 1)*WIDTH / nprocs) + 1], 1, MPI_INT, right, 120, MPI_COMM_WORLD, &status); //the tag says 1 to zero

			}
			else { //odd numbered process
				   //receiving from the previous process and placing it on the left column
				MPI_Recv(&oldMap[i][(myID*WIDTH / nprocs)], 1, MPI_INT, left, 021, MPI_COMM_WORLD, &status); //the tag says zero to 1
																											 //sending the last column to the process on the right (next ID)
				MPI_Send(&oldMap[i][WIDTH*(myID + 1) / nprocs], 1, MPI_INT, right, 122, MPI_COMM_WORLD); //the tag says one to two

																										 //receiving the first column of the right process and placing it after this process's last column
				MPI_Recv(&oldMap[i][((myID + 1)*WIDTH / nprocs) + 1], 1, MPI_INT, right, 221, MPI_COMM_WORLD, &status); //the tag says zero to 1
																														//sending the first column to the left
				MPI_Send(&oldMap[i][(WIDTH*myID / nprocs) + 1], 1, MPI_INT, left, 120, MPI_COMM_WORLD); //the tag says 1 to zero
		}
		}
		//now each thread needs to fill in it's top and bottom extra rows
		//with the extra column included (since the corner cells will require these neighbors)
		//so we subtract 1 from the initial value of j and add 1 to the final value

		// top-bottom boundary conditions
		for (j = (myID*WIDTH / nprocs); j <= (myID + 1)*WIDTH / nprocs + 1; j++) {
			oldMap[0][j] = oldMap[HEIGHT][j]; //the row before the first is the last
			oldMap[HEIGHT + 1][j] = oldMap[1][j]; //the row after the last is the first
		}

		//print();
		//going through the entire 2D array and updating it
		//note that here each process does its own part only and not the entire WIDTHxHEIGHT grid
		//update(myID, nprocs);
		
		//****** replacing update *****
		//for OpenMP to work, the threads shouldnt perform function calls, that is why I'm moving the update code here:
		int numOfThreads = 4;
#pragma omp parallel num_threads(numOfThreads)
		{
			//firstly, let the threads say hello
			if (n == 1 && polite) {
				int tid;
				tid = omp_get_thread_num();
				printf("hi, from cpu %d of %d. I am thread %d of %d\n", myID+1,nprocs, tid+1,numOfThreads);
				fflush(stdout);
			}
#pragma omp for schedule(guided,1)
			for (int i = 1; i <= HEIGHT; i++) {
				
				for (int j = (myID*WIDTH / nprocs) + 1; j <= (myID + 1)*WIDTH / nprocs; j++) {
					//for each cell we count how many neighbors it has
					//array<int, 4> neighborsArr = neighborCount(i, j);
					//replacing neighborCount by its code

					//nFish is the number of neighboring fish, nAdultFish is the number of neighboring adult fish
					//nSharks is the number of neighboring sharks, nAdultSharks is the number of neighboring adult sharks
					int nFish, nAdultFish, nSharks, nAdultSharks = 0;

					//to ensure that the function is not given a boundary cell
					if (i <= 0 || j <= 0 || (i >= HEIGHT + 1) || (j >= WIDTH + 1)) {
						cout << "passed an invalid cell to neighborCount" << endl;
						//return NULL;
					}
					int iMinus, iPlus, jMinus, jPlus;
					iMinus = i - 1;
					iPlus = i + 1;
					jMinus = j - 1;
					jPlus = j + 1;

					//1  2  3
					//4  X  5
					//6  7  8
					//got rid of all the evaluate function calls, and replaced them with:
					//this way when we implement everything in main, no clashes occur between threads of the same process
					array<int, 8> cellNeighbors = { oldMap[iMinus][jMinus], oldMap[i][jMinus], oldMap[iPlus][jMinus] //neighbors 1,2,3
						, oldMap[iMinus][j] , oldMap[iPlus][j]   // neighbors 4 and 5
						, oldMap[iMinus][jPlus], oldMap[i][jPlus] , oldMap[iPlus][jPlus] }; // neighbors 6,7,8

					for (int neighborsCounter = 0; neighborsCounter < 8; neighborsCounter++) {
						//replacing the evaluate function:
						if (cellNeighbors[neighborsCounter] == 0) { //empty cell
							continue;
						}
						else
							if (cellNeighbors[neighborsCounter] > 0) { // fish
								nFish++;
								if (cellNeighbors[neighborsCounter] >= 2) { // adult fish
									nAdultFish++;
								}
							}
							else
								if (cellNeighbors[neighborsCounter] < 0) { //shark
									nSharks++;
									if (cellNeighbors[neighborsCounter] <= -3) { //adult shark
										nAdultSharks++;
									}
								}
					}

					if (oldMap[i][j] > 0) { //fish
											//implementing all the fish rules
											//a fish can die by being eaten, overpopulation, or old age
						if (nSharks >= 5 || nFish == 8 || oldMap[i][j] == 10) {
							newMap[i][j] = 0; //fish dies 
						}
						else {
							//the fish survived, we increment its age
							newMap[i][j] = oldMap[i][j] + 1;
						}
					}
					else if (oldMap[i][j] < 0) { //shark
												 //implementing all the shark rules
						float sharkHeartAttack;
						sharkHeartAttack = rand() / ((float)RAND_MAX + 1);
						//a shark can die by either starvation or randomly or because of old age
						if ((nSharks >= 6 && nFish == 0) || (sharkHeartAttack <= 0.031) || (oldMap[i][j] <= -20)) {
							newMap[i][j] = 0; //shark dies
						}
						else {
							//shark survives, increment age by making the cell's value more negative
							newMap[i][j] = oldMap[i][j] - 1;
						}
					}
					else { //empty
						   //breeding rules
						if (nFish >= 4 && nAdultFish >= 3 && nSharks < 4) {
							newMap[i][j] = 1; //a fish is born
						}
						else if (nSharks >= 4 && nAdultSharks >= 3 && nFish < 4) {
							newMap[i][j] = -1; //a shark is born
						}
						else {
							//nothing is born
							newMap[i][j] = oldMap[i][j]; //or set it to 0 (since this cell was empty and will remain empty)
						}
					}
					//****optional code for testing only
					//oldMap[i][j] = myID;
					//end of optional testing code

					//resetting the numbers back to get ready for the next cell
					nFish = 0;
					nAdultFish = 0;
					nSharks = 0;
					nAdultSharks = 0;
				}
			}
		}
		//now we copy the new map into the old map
		for (i = 1; i <= HEIGHT; i++) {
			for (j = (myID*WIDTH / nprocs) + 1; j <= (myID + 1)*WIDTH / nprocs; j++) {
				oldMap[i][j] = newMap[i][j];
			}
		}

		//if we're in a multiple of speed, let process 0 thread 0 display the current fish and shark count
		if (n%speed == 0) {
			countOceanMembers(myID, nprocs, 0); //TODO tid
			if (myID == 0)
			{
				cout << "in generation " << n << endl;
				//in order to count the fish and the sharks correctly in between frames, we'll need to merge the total of each process
				//cout << "There are: " << countOceanMembers(0, nprocs).first << " fish and " << countOceanMembers(0, nprocs).second << " sharks" << endl;
				//bug found in above line. Deadlock being created: basically thread 0 is going in and waiting to receive nothing
				//so i'll have to call countOceanMembers with all other threads so that they send it their info.


				//cout << "there are: " << analyze().first << " fish and " << analyze().second << " sharks" << endl;
				if (display) {
					print();
				}
			}
		}
	}

	if (myID == 0) {
		//when all the time steps are complete
		t2 = clock();

		//displaying the ascii visualization is only viable when width and height are small enough
		//print();
		
		float diff((float)t2 - (float)t1);
		cout << "Parallel processing using hybrid(OpenMP+MPI) of a " << WIDTH << "x" << HEIGHT << " grid. Performing " << NUMBER_OF_STEPS << " iterations." << endl;
		printf("Processing time %f seconds using %d processes \n", (diff / 1000), nprocs);

	}
	countOceanMembers(myID, nprocs,0);
	fflush(stdout);

	MPI_Finalize();
	//system("pause");
	return 0;
}

void print() {
	for (int i = 1; i < HEIGHT + 1; i++) {
		for (int j = 1; j < WIDTH + 1; j++) {
			//cout << " ";
			if (oldMap[i][j] > 0) {
				cout << "f";// << oldMap[i][j];
			}
			else if (oldMap[i][j] < 0) {
				cout << "s";
			}
			else {
				cout << "-";
			}
			//cout << " ";
		}
		cout << endl;
	}
	cout << endl;

	//**** optional for testing
	/*for (int i = 0; i < HEIGHT + 2; i++) { //TODO: from 1 to height +1  to not display boundaries
	for (int j = 0; j < WIDTH + 2; j++) {
	cout << oldMap[i][j];
	}
	cout << endl;
	}*/
	//end of optional code
}

pair<int, int> analyze() {
	int numOfFish = 0;
	int numOfSharks = 0;
	for (int i = 1; i <= HEIGHT; i++) {
		for (int j = 1; j <= WIDTH; j++) {
			if (oldMap[i][j] < 0) {
				numOfSharks++;
			}
			if (oldMap[i][j] > 0) {
				numOfFish++;
			}
		}
	}
	return pair<int, int>(numOfFish, numOfSharks);
}

pair<int, int> analyzeCurrentProcess(int myID, int nprocs) {
	int numOfSharks = 0;
	int numOfFish = 0;
	for (int i = 1; i <= HEIGHT; i++) {
		//each process should only analyze its subsection
		for (int k = ((myID*WIDTH / nprocs) + 1); k <= WIDTH*(myID + 1) / nprocs; k++) {
			if(oldMap[i][k]>0){
				numOfFish++;
				//cout << "+" << endl;
			}
			if (oldMap[i][k] < 0) {
				numOfSharks++;
				//cout << "-" << endl;
			}
			
		}
	}
	//for debugging only
	//cout << "in process " << myID << " there are " << numOfFish << " fish and " << numOfSharks << " sharks" << endl;
	return pair<int, int>(numOfFish, numOfSharks);
}

//array<int, 4> neighborCount(int i, int j) {
	//code moved to main
//}

pair<int, int> countOceanMembers(int myID, int nprocs, int myThreadID) {
	int totalFish = 0;
	int totalSharks = 0;
	MPI_Request request;
	MPI_Status status;
	//this block only works on small enough values of HEIGHT and WIDTH, when they grow too big, we run out of memory
	//that is why I will implement a different technique for counting total fish and sharks
	/*
	//all the processes send back their parts of the ocean to process 0
	if (myID == 0) {
	//this for loop goes from 1 to number of processes-1 and receives data from every process
	for (int sourceProcess = 1; sourceProcess < nprocs; sourceProcess++) {
	//going through all the i's for every process
	for (i = 1; i <= HEIGHT; i++) {
	//each process should only take 1/nprocs of the width
	for (int k = ((sourceProcess*WIDTH / nprocs) + 1); k <= WIDTH*(sourceProcess + 1) / nprocs; k++) {
	//receive from all the other processes
	MPI_Recv(&oldMap[i][k], 1, MPI_INT, sourceProcess, 123, MPI_COMM_WORLD, &status);
	}
	}
	}
	}
	else {
	//send to process 0 the final ocean
	for (i = 1; i <= HEIGHT; i++) {
	//each process should only send 1/nprocs of the width
	for (int k = ((myID*WIDTH / nprocs) + 1); k <= WIDTH*(myID + 1) / nprocs; k++) {
	MPI_Send(&oldMap[i][k], 1, MPI_INT, 0, 123, MPI_COMM_WORLD);
	}
	}
	}
	*/

	//the alternative way is to let each process count its fish and sharks, and send only the totals to process 0
	if (myID == 0) {
		int tempReceiverFish = 0;
		int tempReceiverSharks = 0;
		totalFish = analyzeCurrentProcess(0, nprocs).first;
		totalSharks = analyzeCurrentProcess(0, nprocs).second;
		
		//receiving all the sums of fish and sharks
		for (int sourceProcess = 1; sourceProcess < nprocs; sourceProcess++)
		{
			//the receives need to be nonblocking to prevent from deadlocks
			MPI_Irecv(&tempReceiverFish, 1, MPI_INT, sourceProcess, 32120, MPI_COMM_WORLD, &request);
			//asking all the other processes for their total fish and shark counts
			//countOceanMembers(sourceProcess, nprocs); //this line prevents deadlock

			MPI_Wait(&request, &status);
			totalFish += tempReceiverFish;
			//cout << "received " << tempReceiverFish << " fish" << endl;
			MPI_Irecv(&tempReceiverSharks, 1, MPI_INT, sourceProcess, 12320, MPI_COMM_WORLD, &request);

			MPI_Wait(&request, &status);
			//cout << "received " << tempReceiverSharks << " sharks" << endl;
			totalSharks += tempReceiverSharks;

			//cout<<"THE NEW TOTALS ARE "<<totalFish<<"   "<<totalSharks<<endl;
			//receive fishcounts from all the other processes
			//MPI_Recv(&tempReceiverFish, 1, MPI_INT, sourceProcess, 32120, MPI_COMM_WORLD, &status);
			//totalFish += tempReceiverFish;
			//receive sharkcounts from all the other processes
			//MPI_Recv(&tempReceiverSharks, 1, MPI_INT, sourceProcess, 12320, MPI_COMM_WORLD, &status);
			//totalSharks += tempReceiverSharks;

		}
		if (myThreadID == 0) {
			cout << "There are: " << totalFish << " fish and " << totalSharks << " sharks" << endl;
		}
	}
	else {
		int fishCount = 0;
		int sharkCount = 0;
		//send to process 0 the total number of fish and sharks in this subsection of the ocean
		fishCount = analyzeCurrentProcess(myID, nprocs).first;
		sharkCount = analyzeCurrentProcess(myID, nprocs).second;
		//nonblocking sending of the number of fish in this process
		//MPI_Isend(&fishCount, 1, MPI_INT, 0, 32120, MPI_COMM_WORLD, &request);
		//MPI_Wait(&request, &status);
		//nonblocking sending of number of sharks in this process
		//MPI_Isend(&sharkCount, 1, MPI_INT, 0, 12320, MPI_COMM_WORLD, &request);
		//MPI_Wait(&request, &status);
		//
		MPI_Send(&fishCount, 1, MPI_INT, 0, 32120, MPI_COMM_WORLD);
		//cout << "sent " << fishCount << " fish" << endl;
		MPI_Send(&sharkCount, 1, MPI_INT, 0, 12320, MPI_COMM_WORLD);
		//cout << "sent " << sharkCount << " sharks" << endl;
	}

	return pair<int, int>(totalFish, totalSharks);
}

void evaluate(int value, int &fish, int &adultFish, int &sharks, int &adultSharks) {
	//cout << "value: " << value << " fish: " << fish << " aFish: " << adultFish << " sharks: " << sharks << endl;
	if (value == 0) { //empty cell
		return;
	}
	if (value > 0) { // fish
		fish++;
		if (value >= 2) { // adult fish
			adultFish++;
		}
	}
	if (value < 0) { //shark
		sharks++;
		if (value <= -3) { //adult shark
			adultSharks++;
		}
	}
}

void update(int myID, int nprocs) {
	//code moved to main
}