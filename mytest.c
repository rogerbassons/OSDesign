#include <stdio.h>
//#include "vm.h"
#include "my_pthread_t.h"

//int NUM_THREADS = 3;
pthread_t pts[3];
int tindex[3];
int mlarp = 0;

void *threadfunc(void *threadnum){
  printf("In thread.\n");
  int * tn = (int *)threadnum;
  printf("Threadnum: %d\n", *tn);
  int * datac = (int *)malloc(200);
  printf("Sucessfully malloc'd. Thread %d given addr %lu\n", *tn, datac);
  *datac = mlarp;
  mlarp++;
  printf("Thread %d had data %d. \n", *tn, *datac);
  my_pthread_yield();
  int * datac2 = (int *)malloc(200);
  printf("Thread %d successfully malloc'd again. Was given addr %lu\n", *tn, datac2);
  *datac2 = mlarp * 10;
  mlarp++;
  printf("Thread %d had second data %d. \n", *tn, *datac2);
  my_pthread_yield();
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
  printf("Main: Finished one round of allocs....\n");
  my_pthread_yield();
}
