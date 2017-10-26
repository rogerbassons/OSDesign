#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"

int nextPage(int current)
{
	int next = current;
	next += mem[next+1]; 
	if (next >= PHYSICAL_SIZE)
		next = next % PHYSICAL_SIZE + 2;
	return next;
}

void *getNextFreePage(size_t size, unsigned pid)
{
	int next = mem[0];

	if (mem[next]) {
		do {
			next = nextPage(next);
		} while (mem[next] && next != mem[0]); 
	}

	void *ptr = NULL;

	if (mem[next] == 0) {
		mem[0] = next;
		mem[next] = pid;
		mem[next + 1] = size;
		mem[next + 2] = size;
		ptr = &mem[next+3];
	}

	return ptr;
}

void *getNextFreeThread(size_t size)
{
	//unsigned pid = (*running)->id;
	unsigned pid = 1;

	int next = 1;
	while (mem[next] != pid) 
		next = nextPage(next);
	//TODO
}

void printMemory(size_t size)
{
	int i,j = 0;
	printf("%i\n", mem[0]);
	for (i = 1; i < PHYSICAL_SIZE; i++) {
		printf("%i ", mem[i]);
		j++;
		if (j > size+2) {
			j = 0;
			printf("\n");
		}
	}
}

void* myallocate (size_t size, char *file, char *line, int request)
{

	if (mem == NULL) {
		mem = calloc(PHYSICAL_SIZE, sizeof(char));
		mem[0] = 1; // next free block start
	}

	void *ptr = NULL;
	switch (request) {
		case THREADREQ:
			// allocate
			ptr = getNextFreeThread(size);
			break;
		default:
			// reserve a page
			ptr = getNextFreePage(size, request);
			printMemory(size);

	}
	return ptr;
}

void mydeallocate(void* ptr, char *file, char *line, int request)
{
	char *c = (char*)ptr - 1;
	*c = 0;
}



