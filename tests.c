#include "tests.h"


void test1(){
	printf("-------------Test 1----------:\n");
	char* ptr = (char*)customMalloc(100);
	printf("pointer is %p\n", ptr);
	ptr = "Noa";
	printf("pointer is %p\n", ptr);
	printf("%s\n", ptr);
	customFree(ptr);
	printf("%s\n", ptr);
	printf("End of Test 1\n");
}


void test2(){
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

void test3(){
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
