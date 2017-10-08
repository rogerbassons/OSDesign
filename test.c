#include "my_pthread_t.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

void *printTest(void *arg)
{
	
	for (int i = 1; i <= 100; i++) {
		i++;
		printf("TEST %i\n", i);
	}
}

void *printHello(void *arg)
{
	for (int i = 1; i <= 100; i++) {
		i++;
		printf("Hello %i\n", i);
	}
}

void *printError(void *arg)
{
	
	for (int i = 1; i <= 100; i++) {
		i++;
		printf("ERROR %i\n", i);
	}
}


int main()
{
	my_pthread_t t1;
	my_pthread_t t2;
	my_pthread_t t3;

	my_pthread_create(&t1, NULL, printTest, NULL);
	my_pthread_create(&t2, NULL, printHello, NULL);
	my_pthread_create(&t3, NULL, printError, NULL);

	int i = 0;
	while(1) {
		printf("MAIN %i\n", i); // will be changed with my_pthread_join
		i++;
	}

       

	return 0;
}
