#include "customAllocator.h"

//----------- in data memory space --------------------//
static void* heapStart = NULL;
static Header* headerList = NULL; // Your sorted list head
static Header* headerListTail = NULL; // Your sorted list tail

// for multi-threaded allocator
static pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;
//static int counter = 0;

//------------------------------------------------//

/*=============================================================================
helper functions
=============================================================================*/
void heapCreate(){
    heapStart = sbrk(0);

    //part B
    pthread_mutex_init(&counterMutex, NULL);
    for(int i = 0; i < INITIAL_REGION_NUM; i++){
        
    }
}
void heapKill(){
    brk(heapStart); //reset brk to initial state
}

void addHeaderToList(Header* newHeader, Header* predecessorHeader){
    if(headerList == NULL){
        headerList = newHeader;
        newHeader->prev = NULL;
        newHeader->next = NULL;
        headerListTail = newHeader;
    } else if(predecessorHeader == NULL){
        //we add to the beginning of the list
        newHeader->next = headerList;
        newHeader->prev = NULL;
        headerList->prev = newHeader;
        headerList = newHeader;
    } else {
        //we get a predecessor header
        newHeader->next = predecessorHeader->next;
        newHeader->prev = predecessorHeader;
        predecessorHeader->next = newHeader;
        if(newHeader->next == NULL){
            headerListTail = newHeader;
        } else{
            newHeader->next->prev = newHeader;
        }
    }
}

void removeHeaderFromList(Header* header){
    if(header->prev != NULL){
        header->prev->next = header->next;
    } else {
        headerList = header->next;
    }
    if(header == headerListTail){
        headerListTail = header->prev;
    } else {
        header->next->prev = header->prev;
    }
}

//recives size includng the header size
bool findBestFit(size_t neededSize, Header** predecessortoBestFit){

    //first find the gap between heapStart and first header
    size_t bestFitSize = (size_t)(-1);
    size_t gap_size = 0;
    if(headerList == NULL){
        gap_size = (size_t)sbrk(0) - (size_t)heapStart;
        if(gap_size >= neededSize){
            *predecessortoBestFit = NULL;
            return true;
        } else {
            return false;
        }
    }
    //check first gap
    //if first gap is best fit, set predecessor to NULL and return true
    gap_size = (size_t)headerList - (size_t)heapStart;
    if(gap_size >= neededSize){
        bestFitSize = gap_size;
        *predecessortoBestFit = NULL;
    }

    Header* current = headerList;
    while(current != NULL){
        gap_size = followingFreeBlockSize(current);
        if(gap_size >= neededSize && gap_size < bestFitSize){
            bestFitSize = gap_size;
            *predecessortoBestFit = current;
        }
        current = current->next;
    }
    if(bestFitSize != (size_t)(-1)){
        return true;
    } else {
        return false;
    }

}

size_t followingFreeBlockSize(Header* header){
    if(header == NULL ){
        return 0;
    }
    size_t blockEnd = (size_t)endOfBlock(header);
    if(header->next == NULL){
        size_t programBreak = (size_t)sbrk(0);
        return programBreak - blockEnd;
    }
    return ((size_t)header->next - blockEnd);
}

void* endOfBlock(Header* header){
    return (void*)((size_t)header + sizeof(Header) + header->size);
}

void outOfMemHandler(){
    brk(heapStart); //reset brk to initial state
    printf("<sbrk/brk error>: out of memory\n");
    exit(1);
}




/*=============================================================================
PART A
=============================================================================*/


void* customMalloc(size_t size){
    if(size == 0){
        return NULL;
    }
    size = ALIGN_TO_MULT_OF_4(size);
    size_t neededSize = size + sizeof(Header);
    Header* predecessorHeader = NULL;
    void* startHeader = NULL;
    bool found_free_block = findBestFit(neededSize, &predecessorHeader);
    printf("found best fit: %s\n", found_free_block ? "true" : "false");
    if(!found_free_block){
        //no free block, raise program break
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
    addHeaderToList(allocatedHeader, predecessorHeader);
    //return pointer to memory after header
    printMemState();
    return (void*)((size_t)allocatedHeader + sizeof(Header));
}

void customFree(void* ptr){
    if(ptr == NULL){
        printMemState();
        return;
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        printf("<free error>: passed non-heap pointer\n");
        printMemState();
        return;
    }
    Header* headerToFree = (Header*)((size_t)ptr - sizeof(Header));
    removeHeaderFromList(headerToFree);
    int err;
    if(endOfBlock(headerToFree) == sbrk(0)){
        if(headerListTail == NULL){
            err = brk(heapStart); //shrink heap to initial state
        } else{
            err = brk(endOfBlock(headerListTail)); //shrink heap to last allocated block
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


/*=============================================================================
DEBUG FUNCTIONS
=============================================================================*/

void printMemState(){
    printf("\n\n\n-------- Memory State --------\n");
    printf("Heap Start: %p\n", heapStart);
    printf("Program Break: %p\n", sbrk(0));
    //Header* current = headerList;
    //int index = 0;
    // while(current != NULL){
    //     printf("Block %d: Header at %p, Size: %zu, End: %p\n", index, (void*)current, current->size, endOfBlock(current));
    //     index++;
    //     current = current->next;
    // }
    printf("-------- End of Memory State --------\n\n\n\n");
}
