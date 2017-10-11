#include "my_pthread_t.h"
#include <stdio.h>

my_pthread_mutex_t m;

void *increment1(void *x)
{
	int *x_ptr = (int *)x;

	my_pthread_mutex_lock(&m);
	for (int i = 0; i < 1000000000; i++) {
		++*x_ptr;
	}
	my_pthread_mutex_unlock(&m);
	printf("x increment finished\n");

	return NULL;

}

void *increment2(void *x)
{
	int *x_ptr = (int *)x;

	my_pthread_mutex_lock(&m);

	for (int i = 0; i < 100000000; i++) {
		++*x_ptr;
	}
	my_pthread_mutex_unlock(&m);
	printf("x increment finished\n");

	return NULL;

}

int main()
{

	int x = 0, y = 0, z = 0;

	printf("x: %d, y: %d\n", x, y);

	my_pthread_t i1, i2;

	my_pthread_mutex_init(&m, NULL);

	my_pthread_create(&i1, NULL, increment1, &x);
	my_pthread_create(&i2, NULL, increment2, &x);

	while(++y < 100);

	printf("y increment finished\n");

	my_pthread_join(i1, NULL);
	my_pthread_join(i2, NULL);
	my_pthread_mutex_destroy(&m);

	printf("x: %d, y: %d\n", x, y);

	return 0;

}
