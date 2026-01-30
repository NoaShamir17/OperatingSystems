#include "customAllocator.h"
#include "tests.h"

int main2();

int main() {
	printf("hello\n");
    heapCreate();
    testBasic1();
    testBasic2();
    testGapAtStart();
    testBestFit();
    testCalloc();
    testRealloc();
    testFreeErr();
    heapKill();
    heapCreate();
    testMTMallocFree();
    heapKill();
    heapCreate();
    main2();
    heapKill();
    return 0;
}
