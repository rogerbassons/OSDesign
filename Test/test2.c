#include "../my_pthread_t.h"
#include <stdio.h>

void *printHello()
{
	int x = 0;
	while(1) {
		printf("Hello %i\n", x);
		x++;
		pthread_yield();
	}
	return NULL;//(void *)50;

}

void *printBye()
{
	int x = 0;
	while(1) {
		printf("Bye %i\n", x);
		x++;
	}
	return NULL;

}


void *printTest()
{
	int x = 0;
	while(1) {
		printf("Test %i\n", x);
		x++;
	}
	return NULL;

}


int main()
{

	pthread_t t1;
	pthread_t t2;
	pthread_t t3;


	pthread_create(&t1, NULL, printHello, NULL);
	pthread_create(&t2, NULL, printBye, NULL);
	pthread_create(&t3, NULL, printTest, NULL);


	pthread_join(t1, NULL);


	return 0;

}
