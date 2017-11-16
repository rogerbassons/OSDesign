#include "../my_pthread_t.h"
#include <stdio.h>
pthread_mutex_t m;

void *test(void *ptr)
{
	int *x = (int *) ptr;
	printf("Pointer: %p\n", x);

	int i;
	//pthread_mutex_lock(&m);
	for (i = 0; i < 10000000; i++) {
		*x += 1;
	}
	//pthread_mutex_unlock(&m);

		
	return NULL;
}

int main()
{

 	int nThreads = 10;
	
	pthread_t t[nThreads];
	pthread_mutex_init(&m, NULL);

	int *x = (int *)shalloc(sizeof(int));
	*x = 0;
	
	int i;
	for (i = 0; i < nThreads; i++) {
		if(pthread_create(&t[i], NULL, test, x)) {
			
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


	printf("Result: %i\n", *x);
	return 0;

}
