#include <stdlib.h>
#include "vm.h"

void *getNextFree(size_t size)
{
	int next = mem[0];
	do {
		next += size;
		if (next >= PHYSICAL_SIZE)
			next = next % PHYSICAL_SIZE + 1;
	} while (mem[next] && next != mem[0]); 

	mem[0] = next;
	mem[next] = 1;
	return &mem[next+1];
}

void printMemory(size_t size)
{
	int i,j = 0;
	printf("%i\n", mem[0]);
	for (i = 1; i < PHYSICAL_SIZE; i++) {
		printf("%i ", mem[i]);
		j++;
		if (j > size) {
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
		case LIBRARYREQ:
			// reserve a page
			ptr = getNextFree(size);
		case THREADREQ:
			// allocate
			break;
	}
	printMemory(size);
	return ptr;
}

void mydeallocate(void* ptr, char *file, char *line, int request)
{
	char *c = (char*)ptr - 1;
	*c = 1;
}



