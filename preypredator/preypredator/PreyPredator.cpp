// PreyPredator.cpp : main project file.
// Defines the entry point for the console application.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <iostream>
#include <time.h> 
#include <array>
using namespace std;

//speed selected will print every nth generation (1,10 or 100)
#define FAST 100
#define MEDIUM 10
#define SLOW 1

//array dimensions
#define HEIGHT 1024
#define WIDTH 2048

//total number of steps to be done
#define NUMBER_OF_STEPS 500

//global map with 2 extra rows and 2 extra columns to deal with boundaries
int oldMap[HEIGHT + 2][WIDTH + 2] = { 0 };

int newMap[HEIGHT + 2][WIDTH + 2];

//ages all the fish and sharks, kills the ones that should die, and spawns the ones that are bred 
//all changes are stored in newMap and then copied into oldMap afterwards
void update();

//displays the whole grid to the console
void print();
//returns the number of fish and sharks as a pair (fish, shark) in the whole ocean
pair<int,int> analyze();
//returns the number of neighboring fish and sharks and specifies how many are adults  of a given cell
//where i and j specify the location of that cell
//the returned format will be in an array of four numbers:
//number of all neighboring fish, number of adult neighboring fish, number of all neighboring sharks, number of adult neighboring sharks)
array<int, 4>  neighborCount(int i, int j);
void evaluate(int value, int &fish, int &adultFish, int &sharks, int &adultSharks);

int main()
{
	clock_t t1, t2;
	t1 = clock();

	//setting the speed
	int speed = FAST;
	
	int i, j = 0;
	float randFloat;
	//initializing the array with 50% fish, 25% sharks, and 25% empty cells
	//note that the borders (2 rows and 2 columns) are left empty as they will be overwritten next
	for (i = 1; i <= HEIGHT; i++) {
		for (j = 1; j <= WIDTH; j++) {
			randFloat = rand() / ((float)RAND_MAX + 1);
			if (randFloat<0.25) {
				//25% are empty
				oldMap[i][j] = 0;
			}
			else if(randFloat>=0.25 && randFloat<0.5) {
				//25% are sharks
				oldMap[i][j] = -1;
			}
			else {
				//50% are fish
				oldMap[i][j] = 1;
			}
		}
	}
	
	//to repeat the simulation NUMBER_OF_STEPS of times
	for (int n = 0; n <= NUMBER_OF_STEPS; n++) {
		//now we need to copy the edges to simulate an infinite ocean
		//starting with the corners (to not go over them twice if we loop vertically and horizontally)
		oldMap[0][0] = oldMap[HEIGHT][WIDTH];
		oldMap[0][WIDTH + 1] = oldMap[HEIGHT][1];
		oldMap[HEIGHT + 1][WIDTH + 1] = oldMap[1][1];
		oldMap[HEIGHT + 1][0] = oldMap[1][WIDTH];
		/* left-right boundary conditions */
		for (i = 1; i <= HEIGHT; i++) {
			oldMap[i][0] = oldMap[i][WIDTH];
			oldMap[i][WIDTH + 1] = oldMap[i][1];
		}
		/* top-bottom boundary conditions */
		for (j = 1; j <= WIDTH; j++) {
			oldMap[0][j] = oldMap[HEIGHT][j];
			oldMap[HEIGHT + 1][j] = oldMap[1][j];
		}

		//print();
		//going through the entire 2D array
		update();
		if (n%speed == 0) {
			cout << "Generation " << n << endl;
			cout << "there are: " << analyze().first << " fish and " << analyze().second << " sharks" << endl;
			//uncomment this on small numbers like 20x50 grids
			//print();
			
		}
	}
	//when all the time steps are complete
	t2 = clock();
	//displaying the ascii visualization is only viable when width and height are small enough
	//print();

	float diff((float)t2 - (float)t1);
	cout << "Serial processing of a " << WIDTH << "x" << HEIGHT << " grid. Performing " << NUMBER_OF_STEPS << " iterations." << endl;
	printf("Processing time %f seconds \n", (diff/1000));
	system("pause");
	return 0;
}

void print() {
	for (int i = 1; i < HEIGHT +1; i++) {  
		for (int j = 1; j < WIDTH+1; j++) {
			//cout << " ";
			if (oldMap[i][j] > 0) {
				cout << "f";// << oldMap[i][j];
			} else if (oldMap[i][j] < 0) {
				cout << "s";
			} else {
				cout << "-";
			}
			//cout << " ";
		}
		cout << endl;
	}
	cout << endl;
}

pair<int, int> analyze() {
	int numOfFish = 0;
	int numOfSharks = 0;
	for (int i = 1; i <= HEIGHT; i++) {
		for (int j = 1; j <= WIDTH; j++) {
			//negative numbers represent sharks, and positive numbers represent fish
			//absolute value represents the age
			if (oldMap[i][j] < 0) {
				numOfSharks++;
			}
			if (oldMap[i][j] > 0) {
				numOfFish++;
			}
		}
	}
	return pair<int,int>(numOfFish, numOfSharks);
}

array<int,4> neighborCount(int i, int j) {
	int fish = 0;
	int adultFish = 0;
	int sharks = 0;
	int adultSharks = 0;

	//to ensure that the function is not given a boundary cell
	if (i <= 0 || j <= 0 || (i >= HEIGHT + 1) || (j >= WIDTH + 1)) {
		cout << "passed an invalid cell to neighborCount" << endl;
		//return NULL;
	}
	//1  2  3
	//4  X  5
	//6  7  8
	evaluate(oldMap[i - 1][j - 1], fish, adultFish, sharks, adultSharks); //neighbor 1
	evaluate(oldMap[  i  ][j - 1], fish, adultFish, sharks, adultSharks); //neighbor 2
	evaluate(oldMap[i + 1][j - 1], fish, adultFish, sharks, adultSharks); //neighbor 3

	evaluate(oldMap[i - 1][j], fish, adultFish, sharks, adultSharks);	  //neighbor 4
	evaluate(oldMap[i + 1][j], fish, adultFish, sharks, adultSharks);	  //neighbor 5

	evaluate(oldMap[i - 1][j + 1], fish, adultFish, sharks, adultSharks); //neighbor 6
	evaluate(oldMap[  i  ][j + 1], fish, adultFish, sharks, adultSharks); //neighbor 7
	evaluate(oldMap[i + 1][j + 1], fish, adultFish, sharks, adultSharks); //neighbor 8

	array<int,4> neighbors = { fish,adultFish,sharks,adultSharks };
	return neighbors;
}

void evaluate(int value, int &fish, int &adultFish, int &sharks, int &adultSharks) {
	//cout << "value: " << value << " fish: " << fish << " aFish: " << adultFish << " sharks: " << sharks << endl;
	if (value == 0) { //empty cell
		return;
	}
	if (value > 0) { // fish
		fish++;
		if (value >= 2){ // adult fish
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

void update() {
	//i and j will be used in the for loops
	//nFish is the number of neighboring fish, nAdultFish is the number of neighboring adult fish
	//nSharks is the number of neighboring sharks, nAdultSharks is the number of neighboring adult sharks
	int i, j, nFish, nAdultFish, nSharks, nAdultSharks = 0;
	for (i = 1; i <= HEIGHT; i++) {
		for (j = 1; j <= WIDTH; j++) {
			//for each cell we count how many neighbors it has
			array<int,4> neighborsArr = neighborCount(i,j);
			nFish = neighborsArr[0];
			nAdultFish = neighborsArr[1];
			nSharks = neighborsArr[2];
			nAdultSharks = neighborsArr[3];

			if (oldMap[i][j] > 0) { //fish
				//implementing all the fish rules
				//a fish can die by being eaten, overpopulation, or old age
				if (nSharks >= 5 || nFish == 8 || oldMap[i][j]>=10) {
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
				if ((nSharks >= 6 && nFish == 0)||(sharkHeartAttack<=0.031) || (oldMap[i][j]<=-20) ) {
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
		}
	}
	//now we copy the new map into the old map
	for (i = 1; i <= HEIGHT; i++) {
		for (j = 1; j <= WIDTH; j++) {
			oldMap[i][j] = newMap[i][j];	
		}
	}
}