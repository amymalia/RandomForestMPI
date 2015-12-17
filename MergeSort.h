/*
 *  MergeSort.h
 *  
 *
 *  Created by Amy Takayesu on 12/15/15.
 *  Copyright 2015 __MyCompanyName__. All rights reserved.
 *
 */

//int a[];

void exchange(int i, int j);

void compare(int i, int j);

/**
 * lo is the starting position and
 * n is the length of the piece to be merged,
 * r is the distance of the elements to be compared
 */
void oddEvenMerge(int lo, int n, int r);

/**
 * sorts a piece of length n of the array
 * starting at position lo
 */
void oddEvenMergeSort(int lo, int n);


int main();