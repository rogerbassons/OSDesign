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
  int * datac = (int *)malloc(20);
  printf("Sucessfully malloc'd.\n");
  *datac = mlarp;
  mlarp++;
  printf("Thread %d had data %d\n", *tn, *datac);
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
}
