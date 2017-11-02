#include "vm.h"

int main()
{
	char *a = myallocate(4000, NULL, NULL, 1);
	int *n = (int *) myallocate(sizeof(int), NULL, NULL, 0);

	*n = 1234;

	printf("Number: %i", *n);
	mydeallocate(a, NULL, NULL, 1);
	return 0;

}
