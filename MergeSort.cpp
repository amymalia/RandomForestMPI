/*
 *  MergeSort.cpp
 *  
 *
 *  Created by Amy Takayesu on 12/15/15.
 *  Copyright 2015 __MyCompanyName__. All rights reserved.
 *
 * The odd-even mergesort algorithm was developed by K.E. Batcher [Bat 68].
 * It is based on a merge algorithm that merges two sorted halves of a
 * sequence to a completely sorted sequence.
 */

#include "MergeSort.h"
#include <stdlib.h>
#include <iostream>
#include <iomanip>


void exchange(int i, int j) {
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
}

void compare(int i, int j) {
    if (a[i] > a[j])
        exchange(i, j);
}

/**
 * lo is the starting position and
 * n is the length of the piece to be merged,
 * r is the distance of the elements to be compared
 */
void oddEvenMerge(int lo, int n, int r) {
    int m = r * 2;
    if (m < n) {
        oddEvenMerge(lo, n, m); // even subsequence
        oddEvenMerge(lo + r, n, m); // odd subsequence
        for (int i = lo + r; i + r < lo + n; i += m)
            compare(i, i + r);
    } else
        compare(lo, lo + r);
}

/**
 * sorts a piece of length n of the array
 * starting at position lo
 */
void oddEvenMergeSort(int lo, int n) {
    if (n > 1) {
        int m = n / 2;
        oddEvenMergeSort(lo, m);
        oddEvenMergeSort(lo + m, m);
        oddEvenMerge(lo, n, 1);
    }
}

int start() {
    int i, n = sizeof(a) / sizeof(a[0]);
    for (i = 0; i < n; i++)
        std::cout << std::setw(3) << a[i];
    std::cout << std::endl;
    oddEvenMergeSort(0, n);
    for (i = 0; i < n; i++)
        std::cout << std::setw(3) << a[i];
    std::cout << std::endl;
	
    return(0);
}

int main() {
	start();
}