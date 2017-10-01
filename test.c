#include "my_pthread_t.h"
#include <stdio.h>

void *printTest(void *arg)
{
  printf("TEST\n"); 
}


int main()
{
  my_pthread_t *t;
  my_pthread_create(t, NULL, printTest, NULL);
  return 0;
}
