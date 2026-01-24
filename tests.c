#include "tests.h"


void test1(){
	char* ptr = (char*)customMalloc(100);
	printf("pointer is %p\n", ptr);
	ptr = "Noa";
	printf("pointer is %p\n", ptr);
	printf("%s\n", ptr);
	customFree(ptr);
	printf("%s\n", ptr);
}


void test2(){

}

void test3{


}
