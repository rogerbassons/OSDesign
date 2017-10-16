// File:	vectorMultiply.c
// Author:	Yujie REN
// Date:	09/23/2017
// Modified by Roger Bassons

#include <stdio.h>
#include <unistd.h>

#include "../my_pthread_t.h"

#define THREAD_NUM 100

#define VECTOR_SIZE 1000000

my_pthread_mutex_t   mutex;

my_pthread_t thread[THREAD_NUM];

int r[VECTOR_SIZE];
int s[VECTOR_SIZE];
int *res;

/* A CPU-bound task to do vector multiplication */
void *vector_multiply(void* arg) {
	int i = 0;
	char *t_name = (char *) arg;
	int n = atoi(t_name) - 1;
	for (i = n; i < VECTOR_SIZE; i += THREAD_NUM) {
		my_pthread_mutex_lock(&mutex);
		*res += r[i] * s[i];
		my_pthread_mutex_unlock(&mutex);		
	}
}


int main() {
	int i = 0;
	res = (int *) malloc(sizeof(int));
	*res = 0;

	my_pthread_mutex_init(&mutex, NULL);

	// initialize data array
	for (i = 0; i < VECTOR_SIZE; ++i) {
		r[i] = i;
		s[i] = i;
	}

	for (i = 0; i < THREAD_NUM; ++i) {
		char *name = (char *) malloc(sizeof(char));
		sprintf(name, "%d", i+1);
		my_pthread_create(&thread[i], NULL, vector_multiply, name);
	}

	for (i = 0; i < THREAD_NUM; ++i)
		my_pthread_join(thread[i], NULL);

	printf("Result: %d\n", *res);

	return 0;
}
