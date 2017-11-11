#include <stdio.h>
//#include "vm.h"
#include "my_pthread_t.h"

//int NUM_THREADS = 3;
pthread_t pts[3];
int tindex[3];

void *threadfunc(void *threadnum){
  printf("In thread.\n");
  int * tn = (int *)threadnum;
  printf("Threadnum: %d\n", *tn);
  char * datac = (char *)malloc(1000);
  *datac = (char)*((int *)threadnum);
  return;
}

int main(){
  int i;
  for(i = 0; i < 3; i++){
    tindex[i] = i + 1;
    printf("Creating thread.\n");
    pthread_create(&pts[i], NULL, &threadfunc, &tindex[i]);
  }
  printMemory();
  my_pthread_yield();
}
