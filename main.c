#include "customAllocator.h"
#include "tests.h"

int main() {
	printf("hello\n");
    heapCreate();
    //test1();
    test2();
    //test3();
    //testCalloc();
    //testRealloc();
    heapKill();
    return 0;
}
