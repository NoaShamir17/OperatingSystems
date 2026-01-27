#include "customAllocator.h"

//----------- in data memory space --------------------//
static void* heapStart = NULL;
static Header* headerList_PartA = NULL; // our sorted list head
static Header* headerListTail_PartA = NULL; // our sorted list tail

// for multi-threaded allocator
static int counter = 0;
static pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;
static int regionCount = 0;
static pthread_mutex_t regionCountMutex = PTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------//

/*=============================================================================
helper functions
=============================================================================*/
void heapCreate(){
    heapStart = sbrk(0);

    //part B
    pthread_mutex_init(&counterMutex, NULL);
    pthread_mutex_init(&regionCountMutex, NULL);

    //allocate initial regions
    void* err = sbrk(REGION_SIZE * INITIAL_REGION_NUM);
    if(err == SBRK_FAIL){
        if(errno == ENOMEM){
            outOfMemHandler();
        } else {
            printf("<sbrk error>: bad sbrk args\n");
            return;
        }
    }

    //update region count
    regionCount = INITIAL_REGION_NUM;

    //initialize mutexes for each region, and their header lists to NULL
    for(int i = 0; i < INITIAL_REGION_NUM; i++){
        RegionHeader* regionHeader = findRegion(i);
        pthread_mutex_init(&(regionHeader->regionMutex), NULL);
        regionHeader->headerList = NULL;
        regionHeader->headerListTail = NULL;
    }
}


void heapKill(){
    //destroy mutexes only for first function call (in part A its called twice - in first malloc and at program end)
    static bool isFirstCall = true;
    if(isFirstCall){
        //destroy all region mutexes
        //first, lock regionCountMutex to read regionCount safely
        pthread_mutex_lock(&regionCountMutex);
        for(int i = 0; i < regionCount; i++){
            RegionHeader* regionHeader = findRegion(i);
            pthread_mutex_destroy(&(regionHeader->regionMutex));
        }
        pthread_mutex_unlock(&regionCountMutex);

        //destroy global mutexes
        pthread_mutex_destroy(&counterMutex);
        pthread_mutex_destroy(&regionCountMutex);
        isFirstCall = false;
    }


    //heap reset
    int err = brk(heapStart); //reset brk to initial state
    if(err == BRK_FAIL){
        if(errno == ENOMEM){
            outOfMemHandler();
        } else {
            printf("<brk error>: bad brk args\n");
            return;
        }
    }

    
}



//------------ Part A helper functions -----------------//

void addHeaderToList(Header* newHeader, Header* predecessorHeader, Header** headerList, Header** headerListTail){
    if(*headerList == NULL){
        *headerList = newHeader;
        newHeader->prev = NULL;
        newHeader->next = NULL;
        *headerListTail  = newHeader;
    } else if(predecessorHeader == NULL){
        //we add to the beginning of the list
        newHeader->next = *headerList;
        newHeader->prev = NULL;
        (*headerList)->prev = newHeader;
        *headerList = newHeader;
    } else {
        //we get a predecessor header
        newHeader->next = predecessorHeader->next;
        newHeader->prev = predecessorHeader;
        predecessorHeader->next = newHeader;
        if(newHeader->next == NULL){
            *headerListTail = newHeader;
        } else{
            newHeader->next->prev = newHeader;
        }
    }
}

void removeHeaderFromList(Header* header, Header** headerList, Header** headerListTail){
    if(header->prev != NULL){
        header->prev->next = header->next;
    } else {
        *headerList = header->next;
    }
    if(header == *headerListTail){
        *headerListTail = header->prev;
    } else {
        header->next->prev = header->prev;
    }
}

//recives size includng the header size
//for part A - regionStart = heapStart, regionEnd = sbrk(0), headerList = headerList_PartA
bool findBestFit(void* regionStart, void* regionEnd, Header* headerList, size_t neededSize, Header** predecessortoBestFit){

    //first find the gap between regionStart and first header
    size_t bestFitSize = (size_t)(-1);
    size_t gap_size = 0;
    if(headerList == NULL){
        gap_size = (size_t)regionEnd - (size_t)regionStart;
        if(gap_size >= neededSize){
            *predecessortoBestFit = NULL;
            return true;
        } else {
            return false;
        }
    }
    //check first gap
    //if first gap is best fit, set predecessor to NULL and return true
    gap_size = (size_t)headerList - (size_t)regionStart;
    if(gap_size >= neededSize){
        bestFitSize = gap_size;
        *predecessortoBestFit = NULL;
    }

    Header* current = headerList;
    while(current != NULL){
        gap_size = followingFreeBlockSize(current, regionEnd);
        if(gap_size >= neededSize && gap_size < bestFitSize){
            bestFitSize = gap_size;
            *predecessortoBestFit = current;
        }
        current = current->next;
    }
    if(bestFitSize != (size_t)(-1)){
        //printf("Best fit found with size %zu\n", bestFitSize); //DEBUG
        return true;
    } else {
        return false;
    }

}


//returns the size of the free block following the given header
//for part A - regionEnd = sbrk(0)
size_t followingFreeBlockSize(Header* header, void* regionEnd){
    if(header == NULL ){
        return 0;
    }
    size_t blockEnd = (size_t)endOfBlock(header);
    if(header->next == NULL){
       // printf("followingFreeBlockSize: header at %p, size %zu, blockEnd %p, programBreak %p, free size %zu\n", (void*)header, header->size, (void*)blockEnd, (void*)programBreak, programBreak - blockEnd); //DEBUG
        return (size_t)regionEnd - blockEnd;
    }
    return ((size_t)header->next - blockEnd);
}

void* endOfBlock(Header* header){
    //printf("endOfBlock: header at %p, size %zu, end at %p\n", (void*)header, header->size, (void*)((size_t)header + sizeof(Header) + header->size)); //DEBUG
    return (void*)((size_t)header + sizeof(Header) + header->size);
}

void outOfMemHandler(){
    heapKill();
    printf("<sbrk/brk error>: out of memory\n");
    exit(1);
}



//------------ Part B helper functions -----------------//

RegionHeader* findRegion(int regionIndex){
    void* regionStart = (void*)((size_t)heapStart + regionIndex * REGION_SIZE);
    return (RegionHeader*)regionStart;
}

void* getRegionStartAddress(int regionIndex){
    return (void*)((size_t)heapStart + regionIndex * REGION_SIZE + sizeof(RegionHeader));
}

void* getRegionEndAddress(int regionIndex){
    return (void*)((size_t)heapStart + (regionIndex + 1) * REGION_SIZE);
}


int getAndIncrementCounter(){
    pthread_mutex_lock(&counterMutex);
    pthread_mutex_lock(&regionCountMutex);
    int current = counter;
    counter = (counter + 1) % regionCount;
    pthread_mutex_unlock(&regionCountMutex);
    pthread_mutex_unlock(&counterMutex);
    return current;
}

void lockRegion(int regionIndex){
    pthread_mutex_t* regionMutex = &(findRegion(regionIndex)->regionMutex);
    pthread_mutex_lock(regionMutex);
}

void unlockRegion(int regionIndex){
    pthread_mutex_t* regionMutex = &(findRegion(regionIndex)->regionMutex);
    pthread_mutex_unlock(regionMutex);
}

bool addNewRegion(){
    pthread_mutex_lock(&regionCountMutex);
    void* err = sbrk(REGION_SIZE);
    //printf("Adding new region at index %d\n", regionCount); //DEBUG
    //printf("sbrk returned %p\n", err); //DEBUG
    if(err == SBRK_FAIL){
        pthread_mutex_unlock(&regionCountMutex);
        if(errno == ENOMEM){
            outOfMemHandler();
        } else {
            printf("<sbrk error>: bad sbrk args\n");
            return false;
        }
    }
    //initialize new region
    RegionHeader* newRegionHeader = findRegion(regionCount);
    pthread_mutex_init(&(newRegionHeader->regionMutex), NULL);
    newRegionHeader->headerList = NULL;
    newRegionHeader->headerListTail = NULL;

    regionCount++;
    pthread_mutex_unlock(&regionCountMutex);
    return true;
}

RegionHeader* getRegionByAdress(Header* addr){
    int regionIndex = ((size_t)addr - (size_t)heapStart) / REGION_SIZE;
    RegionHeader* regionHeader = findRegion(regionIndex);
    return regionHeader; //address not in any region
}

//DEBUG
int getRegionIndexByAdress(Header* addr){
    return ((size_t)addr - (size_t)heapStart) / REGION_SIZE;
}
/*=============================================================================
PART A
=============================================================================*/


void* customMalloc(size_t size){
    static bool isInitialized = false;
    //the first time we call part A customMalloc, we pull PB to heapStart
    if(!isInitialized){
        heapKill(); //reset brk to initial state
        isInitialized = true;
    }

    if(size == 0){
        return NULL;
    }
    size = ALIGN_TO_MULT_OF_4(size);
    size_t neededSize = size + sizeof(Header);
    Header* predecessorHeader = NULL;
    void* startHeader = NULL;
    bool found_free_block = findBestFit(heapStart, sbrk(0), headerList_PartA, neededSize, &predecessorHeader);
    //printf("found best fit: %s\n", found_free_block ? "true" : "false"); //DEBUG
    if(!found_free_block){
        //no free block, raise program break
        predecessorHeader = headerListTail_PartA;
        startHeader = sbrk(neededSize); 
        if(startHeader == SBRK_FAIL){
            if(errno == ENOMEM){
                outOfMemHandler();
            } else {
                printf("<sbrk error>: bad sbrk args\n");
                return NULL;
            }
        }
    }
    else {
        //found free block
        if(predecessorHeader == NULL){
            //free block is at the beginning of the heap
            startHeader = heapStart;
        } else {
            //free block is after predecessorHeader
            startHeader = endOfBlock(predecessorHeader);
        }
    }

    Header* allocatedHeader = (Header*)startHeader;
    //set header info
    allocatedHeader->size = size;
    //add to header list
    addHeaderToList(allocatedHeader, predecessorHeader, &headerList_PartA, &headerListTail_PartA);
    //return pointer to memory after header
    printMemState();
    return (void*)((size_t)allocatedHeader + sizeof(Header));
}

void customFree(void* ptr){
    if(ptr == NULL){
        printf("<free error>: passed null pointer\n");
        printMemState();
        return;
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        printf("<free error>: passed non-heap pointer\n");
        printMemState();
        return;
    }
    Header* headerToFree = (Header*)((size_t)ptr - sizeof(Header));
    removeHeaderFromList(headerToFree, &headerList_PartA, &headerListTail_PartA);
    int err;
    if(endOfBlock(headerToFree) == sbrk(0)){
        if(headerListTail_PartA == NULL){
            err = brk(heapStart); //shrink heap to initial state
        } else{
            err = brk(endOfBlock(headerListTail_PartA)); //shrink heap to last allocated block
        }
        if(err == BRK_FAIL){
            if(errno == ENOMEM){
                outOfMemHandler();
            } else {
                printf("<brk error>: bad brk args\n");
            }
            printMemState();
            return;
        }
    }
    printMemState();
}

void* customCalloc(size_t nmemb, size_t size){
    size_t totalSize = ALIGN_TO_MULT_OF_4(nmemb * size);
    void* allocatedPtr = customMalloc(totalSize);
    if(allocatedPtr == NULL){
        return NULL;
    }
    //initialize memory to zero
    memset(allocatedPtr, 0, totalSize);
    return allocatedPtr;
}

void* customRealloc(void* ptr, size_t size){
    if(ptr == NULL){
        return customMalloc(size);
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        printf("<realloc error>: passed non-heap pointer\n");
        return NULL;
    }
    if(size == 0){
        customFree(ptr);
        return NULL;
    }
    size = ALIGN_TO_MULT_OF_4(size); // align size to multiple of 4
    Header* currentHeader = (Header*)((size_t)ptr - sizeof(Header));
    if(currentHeader->size >= size){
        if(endOfBlock(currentHeader) == sbrk(0)){
            currentHeader->size = size;
            int err = brk(endOfBlock(currentHeader));
            if(err == BRK_FAIL){
                if(errno == ENOMEM){
                    outOfMemHandler();
                } else {
                    printf("<brk error>: bad brk args\n");
                    return NULL;
                }
            }
        } else {
            currentHeader->size = size;
        }
        return ptr;
    } else {
        void* newPtr = customMalloc(size);
        if(newPtr == NULL){
            return NULL;
        }
        //copy old data to new block
        memcpy(newPtr, ptr, currentHeader->size);
        customFree(ptr);
        return newPtr;
    }
    printMemState();
    
}

/*=============================================================================
PART A JOVER
=============================================================================*/

/*=============================================================================
PART B
=============================================================================*/
void* customMTMalloc(size_t size){
    if(size == 0){
        return NULL;
    }
    if(size > 4096){
    	printf("malloc too big\n");//DEBUG
            return NULL;
        }

    size = ALIGN_TO_MULT_OF_4(size);
    size_t neededSize = size + sizeof(Header);

    //find region to allocate from
    //for each region, if we dont find a free block, we move to the next region and add a new region.
    bool found_region = false;
    Header* predecessorHeader = NULL;
    void* startHeader = NULL;

    while(!found_region){
        int regionIndex = getAndIncrementCounter();
        lockRegion(regionIndex);
        RegionHeader* regionHeader = findRegion(regionIndex);
        bool found_free_block = findBestFit(getRegionStartAddress(regionIndex),
                                            getRegionEndAddress(regionIndex),
                                            regionHeader->headerList,
                                            neededSize,
                                            &predecessorHeader);
        if(found_free_block){
            found_region = true;
            //proceed to allocation in this region
            startHeader = NULL;
            if(predecessorHeader == NULL){
                //free block is at the beginning of the region
                startHeader = (void*)((size_t)regionHeader + sizeof(RegionHeader));
            } else {
                //free block is after predecessorHeader
                startHeader = endOfBlock(predecessorHeader);
            }
            Header* allocatedHeader = (Header*)startHeader;
            //set header info
            allocatedHeader->size = size;
            //add to header list
            addHeaderToList(allocatedHeader, predecessorHeader, &(regionHeader->headerList), &(regionHeader->headerListTail));
            //return pointer to memory after header
            printMemState();
            unlockRegion(regionIndex);
            return (void*)((size_t)allocatedHeader + sizeof(Header));
        }
        unlockRegion(regionIndex);
        bool addedNewRegion = addNewRegion(); //adds a new region at the end of the heap
        if(!addedNewRegion){
            return NULL;
        }
    };
    return NULL; //should never reach here
}

void customMTFree(void* ptr){
    if(ptr == NULL){
        printf("<free error>: passed null pointer\n");
        printMemState();
        return;
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        printf("<free error>: passed non-heap pointer\n");
        printMemState();
        return;
    }
    Header* headerToFree = (Header*)((size_t)ptr - sizeof(Header));
    RegionHeader* regionHeader = getRegionByAdress((Header*)headerToFree);
    pthread_mutex_lock(&(regionHeader->regionMutex));
    removeHeaderFromList(headerToFree, &(regionHeader->headerList), &(regionHeader->headerListTail));
    pthread_mutex_unlock(&(regionHeader->regionMutex));
    printMemState();
}

void* customMTCalloc(size_t nmemb, size_t size){
    size_t totalSize = ALIGN_TO_MULT_OF_4(nmemb * size);
    void* allocatedPtr = customMTMalloc(totalSize);
    if(allocatedPtr == NULL){
        return NULL;
    }
    //initialize memory to zero
    memset(allocatedPtr, 0, totalSize);
    return allocatedPtr;
}

//TODO: think of the many edge cases here
void* customMTRealloc(void* ptr, size_t size){
    if(ptr == NULL){
        return customMTMalloc(size);
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        printf("<realloc error>: passed non-heap pointer\n");
        return NULL;
    }
    if(size == 0){
        customMTFree(ptr);
        return NULL;
    }
    size = ALIGN_TO_MULT_OF_4(size); // align size to multiple of 4
    Header* currentHeader = (Header*)((size_t)ptr - sizeof(Header));
    RegionHeader* regionHeader = getRegionByAdress((Header*)currentHeader);
    pthread_mutex_lock(&(regionHeader->regionMutex));
    if(currentHeader->size >= size){
        //just decrease size
        currentHeader->size = size;
        pthread_mutex_unlock(&(regionHeader->regionMutex));
        return ptr;
    } else {
        pthread_mutex_unlock(&(regionHeader->regionMutex));
        void* newPtr = customMTMalloc(size);
        if(newPtr == NULL){
            return NULL;
        }
        //copy old data to new block
        memcpy(newPtr, ptr, currentHeader->size);
        customMTFree(ptr);
        return newPtr;
    }
    printMemState();
    
}

/*=============================================================================
DEBUG FUNCTIONS
=============================================================================*/

//TODO: add global is part B or part A
// if part B go over all regions
void printMemState(){
//    printf("\n\n\n-------- Memory State --------\n");
//    printf("Heap Start: %p\n", heapStart);
//    printf("Program Break: %p\n", sbrk(0));
//    Header* current = headerList_PartA;
//    int index = 0;
//    while(current != NULL){
//        printf("Block %d: Header at %p, Size: %zu, End: %p\n", index, (void*)current, current->size, endOfBlock(current));
//        index++;
//        current = current->next;
//    }
//    printf("-------- End of Memory State --------\n\n\n\n");
}
