#include "vm.h"

int main()
{
	
	char *n1 = myallocate(4000, NULL, 0, 1);
	char *n2 = myallocate(4000, NULL, 0, 2);

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
	
	mydeallocate(n1, NULL, 0, 1);
	mydeallocate(n2, NULL, 0, 1);

	printMemory();
	return 0;

}
