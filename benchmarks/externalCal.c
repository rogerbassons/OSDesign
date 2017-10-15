#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../my_pthread_t.h"


#define THREAD_NUM 20

my_pthread_mutex_t   mutex, nameMutex;

my_pthread_t thread[THREAD_NUM];
long long unsigned int *sum;

void *external_calculate(void* arg) {

	int i = 0, j = 0;
	char *t_name = (char *) arg;

	char filePath[15];
	strcpy(filePath, "./record/");

	FILE *f = fopen(strcat(filePath, t_name), "r");

	char buff[16];
	int nchars = 0;
	int c;
	c = fgetc(f);
	while(c != EOF) {

		buff[nchars] = c;
		nchars++;
		if (c == '\n') {

			my_pthread_mutex_lock(&mutex);
			*sum += strtol(buff, NULL, 10);
			my_pthread_mutex_unlock(&mutex);
			nchars = 0;

		} 
		c = fgetc(f);
	}
	fclose(f);
}


int main() {
	int i = 0;

	sum = (long long int *) malloc(sizeof(long long int));
	*sum = 0;


	my_pthread_mutex_init(&mutex, NULL);


	for (i = 0; i < THREAD_NUM; i++) {
		char *name = (char *) malloc(sizeof(char));
		sprintf(name, "%d", i);
		my_pthread_create(&thread[i], NULL, &external_calculate, name);
	}


	for (i = 0; i < THREAD_NUM; ++i)
		my_pthread_join(thread[i], NULL);


	printf("Total sum: %d\n", *sum);

	return 0;
}
