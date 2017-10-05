#include "my_pthread_t.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

void *printTest(void *arg)
{
	for (int i = 1; i <= 100; i++) {
		i++;
		printf("TEST\n");
	}
}

void *printHello(void *arg)
{
	for (int i = 1; i <= 100; i++) {
		printf("Hello\n");
	}
}

int main()
{
	my_pthread_t t1;
	my_pthread_t t2;

	my_pthread_create(&t1, NULL, printTest, NULL);
	my_pthread_create(&t2, NULL, printHello, NULL);

	while(1); // will be changed with my_pthread_join

       

	return 0;
}
