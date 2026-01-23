#ifndef __CUSTOM_ALLOCATOR__
#define __CUSTOM_ALLOCATOR__

/*=============================================================================
* do no edit lines below!
=============================================================================*/
#include <stddef.h> //for size_t

//Part A - single thread memory allocator
void* customMalloc(size_t size);
void customFree(void* ptr);
void* customCalloc(size_t nmemb, size_t size);
void* customRealloc(void* ptr, size_t size);

//Part B - multi thread memory allocator
void* customMTMalloc(size_t size);
void customMTFree(void* ptr);
void* customMTCalloc(size_t nmemb, size_t size);
void* customMTRealloc(void* ptr, size_t size);

// Part B - helper functions for multi thread memory allocator
void heapCreate();
void heapKill();

/*=============================================================================
* do no edit lines above!
=============================================================================*/

/*=============================================================================
* defines
=============================================================================*/
#define SBRK_FAIL (void*)(-1)
#define ALIGN_TO_MULT_OF_4(x) (((((x) - 1) >> 2) << 2) + 4)

/*=============================================================================
* Block
=============================================================================*/
//Header point to the metadata for each allocated block
// It is stored at the beginning of each block
typedef struct Header
{
    size_t size;
    struct Header* prev;
    struct Header* next;
} Header;

//list of ALLOCATED blocks
//list is sorted by address (header address)

//helper functions for linked list management
void addHeaderToList(Header* newHeader, Header* predecessorHeader);
void removeHeaderFromList(Header* header);
bool findBestFit(size_t neededSize, Header** predecessortoBestFit);
void* endOfBlock(Header* header);



#endif // CUSTOM_ALLOCATOR
