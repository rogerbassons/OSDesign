#include "vm.h"

int main()
{
	char *a = myallocate(10, NULL, NULL, 1);
	int *n = myallocate(sizeof(int), NULL, NULL, 0); 
	return 0;

}
