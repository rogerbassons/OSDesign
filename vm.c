#include <stdlib.h>
void* myallocate (size_t size)
{
	if (mem == NULL) {
		mem = (void *)malloc(PHYSICAL_SIZE);
		mem[0] = 1
	}


	if (size < PHYSICAL_SIZE) {
		int begin = mem[0];
		mem[0] += size;
		return begin;
	} else {
		return NULL;
	}
}

void free (void* ptr)
{

}

