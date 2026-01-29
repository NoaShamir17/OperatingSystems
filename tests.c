#include "tests.h"


void testFreeErr(){
	printf("-------------Test 1----------:\n");
	char* ptr = (char*)customMalloc(100);
	char* ptr2 = ptr;
	printf("pointer is %p\n", ptr);
	ptr = "Noa";
	printf("pointer is %p\n", ptr);
	printf("%s\n", ptr);
	customFree(ptr);
	customFree(ptr2);
	printf("%s\n", ptr);
	printf("End of Test 1\n");
}


void testBasic1(){
	printf("-------------Test 2----------:\n");
	printf("sizeof header: %zu\n", sizeof(Header));
	char* ptr1 = (char*)customMalloc(20);
	char* ptr2 = (char*)customMalloc(30);
	char* ptr3 = (char*)customMalloc(40);
	printf("ptr1: %p, ptr2: %p, ptr3: %p\n", ptr1, ptr2, ptr3);
	customFree(ptr2);
	char* ptr4 = (char*)customMalloc(25);
	printf("ptr4: %p\n", ptr4);
	customFree(ptr1);
	printf("Freed ptr1\n");
	customFree(ptr3);
	printf("Freed ptr3\n");
	customFree(ptr4);
	printf("Freed ptr4\n");
	printf("End of Test 2\n");

}

void testBasic2(){
	printf("-------------Test 3----------:\n");
	char* y = (char*)customMalloc(10);
	*y ='y';
	char* a = (char*)customMalloc(10);
	*a ='a';
	char* i = (char*)customMalloc(10);
	*i ='i';
	char* r = (char*)customMalloc(10);
	*r ='r';
	printf("%c%c%c%c\n", *y, *a, *i, *r);
	//print addresses
	printf("Addresses: y: %p, a: %p, i: %p, r: %p\n", y, a, i, r);
	customFree(a);
	char* o = (char*)customMalloc(5);
	*o ='o';
	printf("%c%c%c%c\n", *y, *o, *i, *r);
	//print addresses
	printf("Addresses: y: %p, o: %p, i: %p, r: %p\n", y, o, i, r);

	customFree(y);
	customFree(i);
	customFree(r);
	customFree(o);
	printf("End of Test 3\n");
}

void testGapAtStart(){
	printf("-------------Test Gap At Start----------:\n");
	char* ptrs[10];
	for(int i = 0; i < 10; i++){
		ptrs[i] = (char*)customMalloc(i * 10);
		printf("Allocated ptrs[%d] at %p\n", i, ptrs[i]);
	}
	customFree(ptrs[0]);
	customFree(ptrs[1]);
	printf("Freed ptrs[0] and ptrs[1]\n");
	customFree(ptrs[4]);
	printf("Freed ptrs[4]\n");
	customFree(ptrs[7]);
	printf("Freed ptrs[7]\n");
	customFree(ptrs[8]);
	printf("Freed ptrs[8]\n");

	void* p1 = customMalloc(15);
	printf("Allocated p1 of size 15 at %p\n", p1);
	void* p2 = customMalloc(10);
	printf("Allocated p2 of size 10 at %p\n", p2);
	void* p3 = customMalloc(100);
	printf("Allocated p3 of size 100 at %p\n", p3);

	customFree(p1);
	customFree(p2);
	customFree(p3);
	customFree(ptrs[2]);
	customFree(ptrs[3]);
	customFree(ptrs[5]);
	customFree(ptrs[6]);
	customFree(ptrs[9]);
	printf("End of Test Gap At Start\n");
}

void testBestFit(){
	printf("-------------Test Fat Beat----------:\n");
	char* ptrs[20];
	for(int i = 0; i < 20; i++){
		ptrs[i] = (char*)customMalloc(200 - i * 10);
		printf("Allocated ptrs[%d] at %p\n", i, ptrs[i]);
	}
	for(int i = 0; i < 20; i += 2){
		customFree(ptrs[i]);
	}

	void* p1 = customMalloc(50); //should fit into ptrs[2]
	printf("Allocated p1 of size 50 at %p\n", p1);
	void* p2 = customMalloc(80); //should fit into ptrs[10]
	printf("Allocated p2 of size 80 at %p\n", p2);
	void* p3 = customMalloc(30); //should fit into ptrs[0]
	printf("Allocated p3 of size 30 at %p\n", p3);

	customFree(p1);
	customFree(p2);
	customFree(p3);
	for(int i = 1; i < 20; i += 2){
		if(ptrs[i] != NULL){
			customFree(ptrs[i]);
		}
	}	
	printf("End of Test Fat Beat\n");
}

void testCalloc(){
	printf("-------------Test Calloc----------:\n");
	int n = 5;
	int* arr = (int*)customCalloc(n, sizeof(int));
	for(int i = 0; i < n; i++){
		printf("arr[%d] = %d\n", i, arr[i]);
	}
	customFree(arr);
	printf("End of Test Calloc\n");
}

void testRealloc(){
	printf("-------------Test Realloc----------:\n");
	int* arr = (int*)customMalloc(3 * sizeof(int));
	for(int i = 0; i < 3; i++){
		arr[i] = i + 1;
	}
	printf("Original array:\n");
	for(int i = 0; i < 3; i++){
		printf("arr[%d] = %d\n", i, arr[i]);
	}
	arr = (int*)customRealloc(arr, 5 * sizeof(int));
	for(int i = 3; i < 5; i++){
		arr[i] = i + 1;
	}
	printf("Resized array:\n");
	for(int i = 0; i < 5; i++){
		printf("arr[%d] = %d\n", i, arr[i]);
	}
	customFree(arr);
	printf("End of Test Realloc\n");
}

//----------------Multi Threaded Tests----------:
#define THREADS_NUM 10
#define ALLOCS_PER_THREAD 100
#define MAX_ALLOC_SIZE 4096 //4KB
void* getRegionEndAddress(int regionIndex);


void testMTMallocFree(){
	printf("-------------Test MT Malloc Free----------:\n");
	
	pthread_t threads[THREADS_NUM];
	for(int i = 0; i < THREADS_NUM; i++){
		pthread_create(&threads[i], NULL, threadAllocFreeRoutine, NULL);
	}
	for(int i = 0; i < THREADS_NUM; i++){
		pthread_join(threads[i], NULL);
	}
	printf("End of Test MT Malloc Free\n");
}

void* threadAllocFreeRoutine(void* arg){
	void* ptrs[ALLOCS_PER_THREAD];
	for(int i = 0; i < ALLOCS_PER_THREAD; i++){
		size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
		ptrs[i] = customMTMalloc(size);
		printf("Thread %lu: Allocated ptrs[%d] of size %zu at %p\n in region %d", pthread_self(), i, size, ptrs[i], getRegionIndexByAdress((Header*)((size_t)ptrs[i] - sizeof(Header))));
		printf("the char is: %c\n", ptrs[i] != NULL ? *((char*)ptrs[i]) : ' '); 
	}
	//write and read to allocated memory
	for(int i = 0; i < ALLOCS_PER_THREAD; i++){
		if(ptrs[i] != NULL){
			*(char*)ptrs[i] = 'a';
			printf("Thread %lu: Wrote to ptrs[%d] at %p\n in region %d", pthread_self(), i, ptrs[i], getRegionIndexByAdress((Header*)((size_t)ptrs[i] - sizeof(Header))));
		}
	}

	for(int i = 0; i < ALLOCS_PER_THREAD; i++){
		customMTFree(ptrs[i]);
		printf("Thread %lu: Freed ptrs[%d] at %p\n in region %d", pthread_self(), i, ptrs[i], getRegionIndexByAdress((Header*)((size_t)ptrs[i] - sizeof(Header))));
	}
	return NULL;
}