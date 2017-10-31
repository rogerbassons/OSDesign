#include "vm.h"

int main()
{
	char *a = myallocate(10, NULL, NULL, 1);
	//int *n = myallocate(sizeof(int), NULL, NULL, 0);
	mydeallocate(a, NULL, NULL, 1);
	return 0;

}
