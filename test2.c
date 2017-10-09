#include "my_pthread_t.h"
#include <stdio.h>

void *printHello()
{
	int x = 0;
	while(x < 2000) {
		printf("test\n");
		x++;
	}
	return NULL;

}

int main()
{

	my_pthread_t t1;

	/* create a second thread which executes inc_x(&x) */
	if(my_pthread_create(&t1, NULL, printHello, NULL)) {

		fprintf(stderr, "Error creating thread\n");
		return 1;

	}


	my_pthread_join(t1, NULL);

	printf("Bye\n");
	return 0;

}
