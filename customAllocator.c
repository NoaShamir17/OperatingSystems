#include "customAllocator.h"
#include <unistd.h> //for sbrk
#include <stdbool.h> //for bool type
#include <string.h> //for memset, memcpy
#include <pthread.h> //for mutex

static void* heapStart = NULL;
static Header* headerList = NULL; // Your sorted list head

/*=============================================================================
helper functions
=============================================================================*/
void heapCreate(){
    heapStart = sbrk(0);
}
void heapKill(){
}

void addHeaderToList(Header* newHeader, Header* predecessorHeader){
    if(headerList == NULL){
        headerList = newHeader;
        newHeader->prev = NULL;
        newHeader->next = NULL;
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
        if(newHeader->next != NULL){
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
    if(header->next != NULL){
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

};

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




/*=============================================================================
PART A
=============================================================================*/

void* customMalloc(size_t size){
    if(size == 0){
        return NULL;
    }

    size_t neededSize = size + sizeof(Header);
    Header* predecessorHeader = NULL;
    void* startHeader = NULL;
    bool found_free_block = findBestFit(neededSize, &predecessorHeader);
    if(!found_free_block){
        //no free block, raise program break
        startHeader = sbrk(neededSize); //TODO: check errno
        if(startHeader == SBRK_FAIL){
            return NULL;
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
    return (void*)((size_t)allocatedHeader + sizeof(Header));
}

void customFree(void* ptr){
    if(ptr == NULL){
        return;
    }
    if((size_t)ptr < (size_t)heapStart || (size_t)ptr >= (size_t)sbrk(0)){
        return;
    }
    Header* headerToFree = (Header*)((size_t)ptr - sizeof(Header));
    removeHeaderFromList(headerToFree);
    if(endOfBlock(headerToFree) == sbrk(0)){ // TODO: check if its sbrk(0) - 1 or + 1
        int err = brk(headerToFree); //TODO: check if its brk() - 1 or + 1
        if(err != 0){//TODO check errno
            printf("brk failed in customFree\n");
        }
    }
}

void* customCalloc(size_t nmemb, size_t size){
    size_t totalSize = nmemb * size;
    void* allocatedPtr = customMalloc(totalSize);
    if(allocatedPtr == NULL){
        return NULL;
    }
    //initialize memory to zero
    memset(allocatedPtr, 0, totalSize);
    return allocatedPtr;
}


