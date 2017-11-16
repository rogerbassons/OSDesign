#include "vm.h"

int main()
{
	size_t maxMainMemory = PHYSICAL_SIZE - MEMORY_START;
	size_t current = 0;
	int i = 1;
	while(current < maxMainMemory - 4000) {
		char *n = myallocate(4000, NULL, 0, i);
		i++;
		current += 4096;
	}
	char *n = myallocate(3960, NULL, 0, i);
	n = myallocate(3960, NULL, 0, i);

	//printMemory();
	printSwap();

	return 0;

}
