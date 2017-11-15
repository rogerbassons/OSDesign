#include "../my_pthread_t.h"
#include <stdio.h>

void *test(void *p)
{
	printf("Allocating...\n");
	int *x = (int *)malloc(sizeof(int));
	printf("Pointer: %p\n", x);
	*x = 0;
	int i;
	
	for (i = 0; i < 10000; i++) {
		*x += 1;
	}
	printf("Result: %i\n", *x);
	//printMemory();
		
	return NULL;
}

int main()
{

 	int nThreads = 5;
	
	pthread_t t[nThreads];


	int i;
	for (i = 0; i < nThreads; i++) {
		if(pthread_create(&t[i], NULL, test, NULL)) {
			
			fprintf(stderr, "Error creating thread\n");
			return 1;
			
		}
	}


	for (i = 0; i < nThreads; i++) {
		if(pthread_join(t[i], NULL)) {

			fprintf(stderr, "Error joining thread\n");
			return 2;

		}
	}



	return 0;

}
