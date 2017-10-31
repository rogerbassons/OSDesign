#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <math.h>

#define GetBit(var, bit) ((var & (1 << bit)) != 0) // Returns true / false if bit is set
#define SetBit(var, bit) (var |= (1 << bit))
#define FlipBit(var, bit) (var ^= (1 << bit))

int findFreePage(size_t size)
{
	int next = -1;
	int i = 1;
	while (i < mem[0] && next == -1) {
		char *c = &mem[i];
		int bit = 0;
		while (bit < 8 && next == -1) {
			if ((*c & (1 << bit)) == 0) {
				next = mem[0] + (i-1) * size;
				(*c |= (1 << bit));
			}
			bit++;
		}
		i++;
	}
	return next;
}

void createSpace(int free, int offset, size_t size)
{
	char *c = &mem[offset];

	int f = size - 1;
	(*c |= (1 << 0)); // set segment is free (1)

	int j = 1;
	while(j < 8 && f != 0) {

		int r = f % 2;
		f /= 2;

		(*c |= (r << j)); // write segment size at  [1 , 7] bits

		j++;
	     		
	}
}


void *getFreePage(size_t size, unsigned pid)
{
	int i = findFreePage(size);
	
	if (i == -1) {
		return NULL;
	} else {
		
		createSpace(1, i, size);
		//setProcessPage(pid);
		return &mem[i];
	}
}

void *getFreeThread(size_t size)
{
	//unsigned pid = (*running)->id;
	unsigned pid = 1;

	unsigned page = mem[0]; //getProcessPage(pid); //TODO

	char *c = &mem[page]; 

	int s = 0;
	int j = 1;
	while(j < 8) {
		if ((*c & (1 << j)) != 0)
			s += pow(2,j-1);
		j++;
	     		
	}


	//TODO
}

void printMemory(size_t size)
{
	int i = 0;
	printf("Bitmap size: %u\n", mem[0]);
	printf("Bitmap:\n---\n");
	for (i = 1; i < mem[0]; i++) {
		char *c = &mem[i];
		int bit;
		for (bit = 0; bit < 8; bit++) {
			printf("%i ", ((*c & (1 << bit)) != 0));
		}
	}
	printf("\n---\n");

	int page = 1;
	int j = 0;
	printf("Page %i: ", page);
	for (i = mem[0]; i < PHYSICAL_SIZE; i++) {
		if (j == 0) {
			printf("-- Free: ");
			char *c = &mem[i];
			int bit;
			for (bit = 0; bit < 8; bit++) {
				printf("%i ", ((*c & (1 << bit)) != 0));
			}
			printf(" -- \n");
		}
		else 
			printf("%i ", mem[i]);
		j++;
		if (j > size - 1) {
			j = 0;
			printf("\n\n");
			page++;
			printf("Page %i: ", page);
		}
	}
	printf("\n");
}

void disableInvalidBits(int invalid)
{
	int i = mem[0] - 1;
	int bits = 0;
	while (invalid > 0 && i > 0 && bits < invalid) {
		char *c = &mem[i];
		int bit = 7;
		while (bits < invalid && bit >= 0) {
			(*c |= (1 << bit));
			bits++;
			bit--;
		}
		i--;		
	}
}


void *myallocate (size_t size, char *file, char *line, int request)
{

	if (mem == NULL) {
		mem = calloc(PHYSICAL_SIZE, sizeof(char));
		
		double pages = ceil((double)PHYSICAL_SIZE / size);
		unsigned bitmapSize = ceil(pages / 8);
		mem[0] = (int) bitmapSize + 1;
		
		printf("Bitmap size: %u\n", bitmapSize);
		
		unsigned invalid = bitmapSize * 8 - pages;
		printf("%i invalid bitmap bits\n", invalid);
		
		disableInvalidBits(invalid);

	}

	void *ptr = NULL;
	switch (request) {
		case THREADREQ:
			// allocate
			ptr = getFreeThread(size);
			break;
		default:
			// reserve a page
			ptr = getFreePage(size, request);
			printMemory(size);

	}
	return ptr;
}

void mydeallocate(void* ptr, char *file, char *line, int request)
{
	switch (request) {
		case THREADREQ:
			// deallocate

			break;
		default:
			// deallocate a page
			printMemory(0);

			
	}
}



