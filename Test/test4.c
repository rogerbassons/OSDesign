#include "../my_pthread_t.h"
#include <stdio.h>

void *test()
{
	int *x = (int *)malloc(sizeof(int));
	*x = 0;
	while (1)
		*x += 1;
	pthread_exit(NULL);
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
		if(pthread_join(&t[i], NULL)) {

			fprintf(stderr, "Error joining thread\n");
			return 2;

		}
	}



	return 0;

}
