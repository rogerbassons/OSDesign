#include "vm.h"

int main()
{
	char *a = myallocate(4000, NULL, 0, 1);
	int *n = (int *) malloc(sizeof(int));

	*n = 1234;

	printf("Number: %i", *n);

	
	free(n);
	mydeallocate(a, NULL, 0, 1);
	return 0;

}
