#include <stdio.h>
//#include "vm.h"
#include "my_pthread_t.h"

//int NUM_THREADS = 3;
pthread_t pts[2];
int tindex[2];
int mlarp = 0;
long thread1addr[750];
long thread2addr[750];
size_t maxMainMemory = PHYSICAL_SIZE - MEMORY_START;
int maxPages;
pthread_mutex_t mutex;
int goint;

void *threadfunc(void *threadnum){
  my_pthread_mutex_lock(&mutex);
  int *tn = (int *)threadnum;
  
  printf("In thread %d. Should allocate %d times\n", *tn, goint );
  int j = 0;
  while(j < goint ){
    //printf("%d about to allocate.\n", *tn);
    int *n = malloc(4000);
    //printf("%d Successfully allocated.\n", *tn);
    if(n == NULL){
      printf("%d allocate failed. j was %d\n",*tn, j);
    }
    
    if(*tn == 0){
      *n = j * 10;}
    else{
      *n = j * -10;}
      
    if(*tn == 0){
      thread1addr[j] = (long)n;
    } else {
      thread2addr[j] = (long)n;
    }
    j++;
  }
  printf("%d about to yield.\n", *tn);
  my_pthread_yield();
  printf("%d came back from yield.\n", *tn);
  if(*tn == 0){
    printf("Dif between last given addr and first: %d\n", (thread1addr[j] - thread1addr[1]) );
  }
  
  printf("About to start data check...\n");
  j = 0;
  for(j = 0; j < goint; j++){
    if(*tn == 0){
      if(*(int *)thread1addr[j] != j * 10){
	printf("Unexpected data: thread1addr[j] = %d, *(int *)thread1addr[j] = %d, expected = %d\n", thread1addr[j], *(int *)thread1addr[j], j * 10);
      }
    }
    else {
      if(*(int *)thread2addr[j] != j * -10){
	printf("Unexpected data: thread2addr[j] = %d, *(int *)thread2addr[j] = %d, expected = %d\n", thread2addr[j], *(int *)thread2addr[j], j * 10); 
      }
    }
  }
  printf("Finished data check.\n");
  my_pthread_mutex_unlock(&mutex);
  my_pthread_exit(NULL);
  //my_pthread_yield();
  return;
}

int main(){
  maxPages = maxMainMemory / 4096;
  goint = (maxPages /2) + 2;
  //goint = 5;
  my_pthread_mutex_init(&mutex, NULL);
  int i = 0;
  while(i < 2){
    tindex[i] = i;
    printf("Creating thread.\n");
    my_pthread_create(&pts[i], NULL, &threadfunc, &tindex[i]);
    i++;
  }
  //printMemory();
  //my_pthread_yield();
  //my_pthread_yield();
  //my_pthread_yield();
  //my_pthread_yield();
  my_pthread_join(pts[0], NULL);
  my_pthread_join(pts[1], NULL);
  printf("main about to print 10 pages in swapf.\n");
  printSwapF(10);
  printf("main: donezo.\n");
  return 0;
}
