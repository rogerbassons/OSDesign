#include "vm.h"

int main()
{
	char *n = myallocate(4000, NULL, 0, 1);

	printMemory();

	int *a = (int *) malloc(sizeof(int));
	*a = 1234;
	printf("Number 1: %i\n", *a);

	printMemory();

	int *b = (int *) malloc(sizeof(int));
	*b = 1;
	printf("Number 2: %i\n", *b);

	printMemory();

	free(b);
	free(a);

	printMemory();
	
	mydeallocate(n, NULL, 0, 1);

	printMemory();
	return 0;

}
