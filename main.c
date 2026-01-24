#include "customAllocator.h"
#include "tests.h"

int main() {
	printf("hello\n");
    heapCreate();
    testGapAtStart();
    testBestFit();
    heapKill();
    return 0;
}
