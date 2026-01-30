#ifndef __CUSTOM_ALLOCATOR__
#define __CUSTOM_ALLOCATOR__
#define _DEFAULT_SOURCE  // Tells the compiler to include BSD/SVID functions like sbrk
#include <unistd.h> //for sbrk
#include <stdbool.h> //for bool type
#include <string.h> //for memset, memcpy
#include <pthread.h> //for mutex
#include <stdio.h> //for printf
#include <errno.h> //for errno
#include <unistd.h>//for brk, sbrk
#include <stdlib.h>//for exit

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

/**
 * Initializes the heap for the custom allocator.
 * Sets up the initial heap start, initializes mutexes for multi-threading,
 * allocates initial regions, and sets up region headers.
 */
void heapCreate();

/**
 * Cleans up the heap and destroys all mutexes.
 * Resets the program break to the initial heap start.
 * Destroys region mutexes on first call (for non-multithread program - called on first malloc).
 */
void heapKill();

/*=============================================================================
* do no edit lines above!
=============================================================================*/

/*=============================================================================
* defines
=============================================================================*/
#define SBRK_FAIL (void*)(-1)
#define BRK_FAIL -1
#define ALIGN_TO_MULT_OF_4(x) (((((x) - 1) >> 2) << 2) + 4)

// Region size = mutex size + pointer to first header + pointer to last header + header size  + 4KB region size
//we add the sizeof(header) bc max block allocated is 4KB, and we want to be able to allocate a block of 4KB in the region + its header
#define REGION_SIZE (sizeof(RegionHeader) + sizeof(Header) + (1 << 12)) 
#define INITIAL_REGION_NUM 8 //minimum number of regions to allocate at heap creation
/*=============================================================================
* Structs
=============================================================================*/

//Header point to the metadata for each allocated block
// It is stored at the beginning of each block
typedef struct Header
{
    size_t size;
    struct Header* prev;
    struct Header* next;
} Header;

typedef struct RegionHeader{
    pthread_mutex_t regionMutex;
    Header* headerList;
    Header* headerListTail;
} RegionHeader;

//helper functions for linked list management
//Protected on the caller side in multi-threaded functions

/**
 * Adds a new header to the sorted doubly-linked list of headers.
 * Maintains the list in order by insertion position.
 * @param newHeader The header to add to the list.
 * @param predecessorHeader The header that should precede the new header, or NULL if at start.
 * @param headerList Pointer to the head of the header list.
 * @param headerListTail Pointer to the tail of the header list.
 */
void addHeaderToList(Header* newHeader, Header* predecessorHeader, Header** headerList, Header** headerListTail);

/**
 * Removes a header from the doubly-linked list of headers.
 * Updates the list pointers accordingly.
 * @param headerToRemove The header to remove from the list.
 * @param headerList Pointer to the head of the header list.
 * @param headerListTail Pointer to the tail of the header list.
 */
void removeHeaderFromList(Header* headerToRemove, Header** headerList,  Header** headerListTail);

//helper functions for memory management
/**
 * Finds the best-fit free block in the given region for the requested size.
 * Searches through gaps between allocated blocks to find the smallest suitable gap.
 * @param regionStart Start address of the region to search (heapStart for part A).
 * @param regionEnd End address of the region to search (sbrk(0) for part A).
 * @param headerList Head of the sorted header list for the region.
 * @param neededSize Total size needed including header.
 * @param predecessortoBestFit Pointer to store the predecessor header of the best fit, or NULL.
 * @return true if a suitable free block is found, false otherwise.
 */
bool findBestFit(void* regionStart, void* regionEnd, Header* headerList, size_t neededSize, Header** predecessortoBestFit);

/**
 * Calculates the end address of a block given its header.
 * @param header The header of the block.
 * @return The address right after the block.
 */
void* endOfBlock(Header* header);

/**
 * Calculates the size of the free block following a given header.
 * @param header The header after which to calculate the free block size.
 * @param regionEnd The end address of the region (sbrk(0) for part A).
 * @return The size of the free block following the header.
 */
size_t followingFreeBlockSize(Header* header, void* regionEnd);

/**
 * Handles out-of-memory situations by cleaning up and exiting.
 */
void outOfMemHandler();

/**
 * Prints the current state of the memory (debug function).
 */
void printMemState();

//part B helper functions
/**
 * Finds the region header for a given region index.
 * @param regionIndex The index of the region.
 * @return Pointer to the RegionHeader for the specified region.
 */
RegionHeader* findRegion(int regionIndex);

/**
 * Atomically gets and increments the global counter for round-robin region selection.
 * @return The current value of the counter before incrementing.
 */
int getAndIncrementCounter();

/**
 * Locks the mutex for a specific region.
 * @param regionIndex The index of the region to lock.
 */
void lockRegion(int regionIndex);

/**
 * Unlocks the mutex for a specific region.
 * @param regionIndex The index of the region to unlock.
 */
void unlockRegion(int regionIndex);

/**
 * Gets the start address of the usable memory in a region (after the region header).
 * @param regionIndex The index of the region.
 * @return The start address of usable memory in the region.
 */
void* getRegionStartAddress(int regionIndex);

/**
 * Gets the end address of a region.
 * @param regionIndex The index of the region.
 * @return The end address of the region.
 */
void* getRegionEndAddress(int regionIndex);

/**
 * Adds a new region to the heap by extending it with sbrk.
 * Initializes the new region's header and mutex.
 * @return true if the region was successfully added, false otherwise.
 */
bool addNewRegion();

/**
 * Gets the region header for the region containing a given address.
 * @param addr The address within the region.
 * @return Pointer to the RegionHeader for the region containing the address.
 */
RegionHeader* getRegionByAdress(Header* addr);

/**
 * Gets the region index for the region containing a given address (debug function).
 * @param addr The address within the region.
 * @return The index of the region containing the address.
 */
int getRegionIndexByAdress(Header* addr);


#endif // CUSTOM_ALLOCATOR
