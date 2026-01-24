#include "customAllocator.h"

int main() {
    heapCreate();
    void* ptr = customMalloc(100);
    customFree(ptr);
    heapKill();
    return 0;
}