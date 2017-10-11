#include "my_pthread_t.h"
#include <stdio.h>

void *printHello()
{
	int x = 0;
	while(x < 1000) {
		x++;
	}
	return NULL;//(void *)50;

}

void *printBye()
{
	int x = 0;
	while(x < 10000000) {
		x++;
	}
	return NULL;

}

int main()
{

	my_pthread_t t1;
	my_pthread_t t2;


	if(my_pthread_create(&t1, NULL, printHello, NULL)) {

		fprintf(stderr, "Error creating thread\n");
		return 1;

	}


	my_pthread_create(&t2, NULL, printBye, NULL);

	void *p;

	my_pthread_join(t1, NULL);
	my_pthread_join(t2, NULL);

	//printf("%d\n", *(int *)p);

	return 0;

}
