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
    testMTMallocFree();
    main2();
    heapKill();
    return 0;
}
