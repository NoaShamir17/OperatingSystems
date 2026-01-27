#include "customAllocator.h"
#include "tests.h"

int main2();

int main() {
	printf("hello\n");
    heapCreate();
    main2();
    heapKill();
    return 0;
}
