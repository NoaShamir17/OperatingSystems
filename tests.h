#ifndef TESTS_H
#define TESTS_H
#include "customAllocator.h"


void testFreeErr();
void testBasic1();
void testBasic2();
void testGapAtStart();
void testBestFit();
void testCalloc();
void testRealloc();
void testMTMallocFree();
void* threadAllocFreeRoutine(void* arg);


#endif
