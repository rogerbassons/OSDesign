#include "../my_pthread_t.h"
#include <stdio.h>
#include <string.h>

void *test(void *p)
{

	int max = 4000;
	
	int size = sizeof(char) * max;
	char *buff = malloc(size);


	printMemory();
	strcpy(buff, "a");
	int i;
	for (i = 0; i < size-2; i++) {
		strcat(buff, "a");
	}
	printf("Result (%i characters): %s\n", i+1,buff);
	return NULL;
}

int main()
{

 	int nThreads = 10;
	
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
