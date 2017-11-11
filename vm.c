#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE 0
#define ELEMENT 1
#define MAX_NUM_THREADS 100


static char mem[PHYSICAL_SIZE] = "";
int inMemFun = 0;
int PAGE_SIZE;
int threadOrders[MAX_NUM_THREADS];

typedef struct spaceNode {
	unsigned pid;
	unsigned free;
	char *start;
	unsigned size;
        int order;
	struct spaceNode *next;
	struct spaceNode *prev;
} SpaceNode;


SpaceNode *getFirstPage()
{
	return (SpaceNode *) &mem[MEMORY_START];
}

SpaceNode *getNextSpace(SpaceNode *old)
{
	if (old == NULL)
		return NULL;
	else
		return (SpaceNode *) old->next;
}

SpaceNode *findFreeSpace(SpaceNode *n, size_t size)
{
	while (n != NULL && !(n->free)) 
		n = getNextSpace(n);

	if (n != NULL && n->free && n->size < size)
		n = getNextSpace(n);
		

	return n;
	
}

int initializeSpace(SpaceNode *n)
{

	
	SpaceNode new;

	new.free = 1;
	new.next = new.prev = NULL;
	new.size = n->size - sizeof(SpaceNode);
	new.start = n->start + sizeof(SpaceNode);
	new.pid = 0;

	memcpy(n->start, &new, sizeof(SpaceNode));

	return 0;
}

int createSpace(SpaceNode *n, size_t size, int type)
{
	if (!n->free || n->size < size)
		return 1;
	
	int restSize = n->size - size;
	if (restSize > sizeof(SpaceNode)) {

		SpaceNode new;
		new.free = 1;
		new.prev = n;
		new.next = n->next;
		new.pid = 0;
		new.size = restSize;

		char *newPos = n->start + size;
		new.start = newPos + sizeof(SpaceNode);
		
		memcpy(newPos, &new, sizeof(SpaceNode));
		
		n->size = size;
		n->next = (SpaceNode *) newPos;
	} 
		    

	n->free = 0;

	if (type == PAGE)
		initializeSpace(n); // create a free node inside the page
			
	return 0;
}


void setProcessPage(SpaceNode *n, int pid)
{
	n->pid = pid;
}

void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreeSpace(getFirstPage(), size);
	
	if (n == NULL) {
		fprintf(stderr, "Error no free pages for process %i\n", pid);
		return NULL;
	} else {
		if (createSpace(n, size, PAGE)) {
			fprintf(stderr, "Error creating space for process %i\n", pid);
			return NULL;
		}
			
		setProcessPage(n, pid);
		return (void *)n;
	}
}

SpaceNode *findProcessPage(unsigned pid)
{
	SpaceNode *n = getFirstPage();
	
	while (n != NULL && n->pid != pid) {
		n = getNextSpace(n);
	}

	if (n != NULL && n->pid == pid)
		return n;
	else
		return NULL;
}

void *getFreeOSElement(size_t size)
{
	SpaceNode *n = findFreeSpace((SpaceNode *)(&mem[1]), size);

	if (n == NULL) {
		return NULL;
	} else {
		
		if (createSpace(n, size, ELEMENT)) {
			perror("Error creating OS space");
			return NULL;
		}
			
		return (void *)n->start;
	}
}

void *getFreeElement(size_t size)
{
	//unsigned pid = (*running)->id;
	unsigned pid = 1;

	SpaceNode * p = findProcessPage(pid);
	if (p == NULL) {
		perror("Cannot find process page");
		return NULL;
	}
	SpaceNode *n = findFreeSpace((SpaceNode *)(p->start), size);
	
	if (n == NULL) {
		return NULL;
	} else {
		
		if (createSpace(n, size, ELEMENT)) {
			perror("Error creating space");
			return NULL;
		}
			
		return (void *)n->start;
	}
}

void printOSMemory()
{
	printf("\n\nOS Memory: \n----------------------------------\n");
	SpaceNode *n = (SpaceNode *) &mem[1];
	int i = 1;
	while (n != NULL) {
		if (n->free)
			printf("----- Free Space -----\n");
		else {
			printf("----- OS Element %i -----\n", i);
			i++;
		}
		printf("Size: %i\n", n->size);
		
		n = getNextSpace(n);	


	}
	printf("----------------------------------\n");
}

void printMemory()
{
	printOSMemory();
	SpaceNode *n = getFirstPage();
	printf("Memory: \n----------------------------------\n");
	int i = 1;
	while (n != NULL) {
		if (n->free)
			printf("----- Free Space -----\n");
		else {
			printf("----- Page %i -----\n", i);
			i++;
		}
		printf("Size: %i\n", n->size);
		if (!n->free) {
			printf("      Contents:\n");

			SpaceNode *s = (SpaceNode *) n->start;
			int j = 1;
			while (s != NULL) {
				if (s->free)
					printf("      ----- Free Space -----\n");
				else {
					printf("      ----- Element %i -----\n", j);
					j++;
				}
				printf("      Size: %i\n", s->size);
				s = getNextSpace(s);
			}
		     
			
			printf("\n");
		}
		n = getNextSpace(n);	


	}
	printf("----------------------------------\n\n\n");

}

void *translatePtr(void * ptr){
  SpaceNode * snptr = ((SpaceNode *)ptr) - 1;
  snptr->order = threadOrders[snptr->pid];
  threadOrders[snptr->pid] += (snptr->size / PAGE_SIZE) + 1;
  void * virloc = (void *) (&mem[MEMORY_START] + (PAGE_SIZE * snptr->order) + sizeof(SpaceNode));
  return virloc;
}

void *myallocate (size_t size, char *file, int line, int request)
{

	if (mem[0] == 0) { //first run
		mem[0] = 1;

		//SOME OTHER INIT STUFF I NEED TO DO:
		PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
		int i;
		for(i = 0; i < MAX_NUM_THREADS; i++){
		  threadOrders[i] = 0;
		}

		//saSeg.sa_flags = SA_SIGINFO;
		//sigemptyset(&saSeg.sa_mask);
		//saSeg.sa_sigaction = memfun;
		//sigaction(SIGSEGV, &saSeg, NULL);

		// System reserved space
		SpaceNode new;
		new.free = 1;
		new.next = new.prev = NULL;
		new.size = MEMORY_START - sizeof(SpaceNode) - 1;
		new.start = &mem[1] + sizeof(SpaceNode);
		new.pid = 0;

		memcpy(&mem[1], &new, sizeof(SpaceNode));



		// Free space for pages
		
		new.free = 1;
		new.next = new.prev = NULL;
		new.size = PHYSICAL_SIZE - MEMORY_START - sizeof(SpaceNode);
		new.start = &mem[MEMORY_START] + sizeof(SpaceNode);
		new.pid = 0;

		memcpy(&mem[MEMORY_START], &new, sizeof(SpaceNode));
	}

	void *ptr = NULL;
	switch (request) {
		case THREADREQ:
			// allocate
			ptr = getFreeElement(size);
			//ptr = translatePtr(ptr);
			break;
	        case OSREQ:
			// allocate memory to OS data
			ptr = getFreeOSElement(size);
			break;
	        case SWAPREQ:
		        //basically, allocate without doing a memprotect or pointer translation.
		        ptr = getFreeElement(size);
			break;
	        default:
			// reserve a page
			ptr = getFreePage(size, request);
	}
	return ptr;
}

int removeSpace(void *ptr, int type)
{
	SpaceNode *n;

	if (type == PAGE)
		n = (SpaceNode *) ptr;
	else
		n = ptr - sizeof(SpaceNode);

	if (n->free)
		return 0;

	SpaceNode *a = n->prev;
	SpaceNode *b = n->next;
	if (a == NULL && b == NULL) {

		n->free = 1;
		
	} else if (a == NULL && b != NULL) {
		
		n->free = 1;
		if (b->free) {
			n->size = b->size + sizeof(SpaceNode);
			n->next = b->next;
		}

		
	} else if (a != NULL && b == NULL) {
		
		if (a->free) {
			a->size = n->size + sizeof(SpaceNode);
			a->next = n->next;
		} else
			n->free = 1;
		
			
	} else if (a != NULL && b != NULL) {
		
		if (!a->free && !b->free) {
			
			n->free = 1;
			
		} else if (!a->free && b->free) {
			
			n->size = b->size + sizeof(SpaceNode);
			n->next = b->next;
			
		} else if (a->free && !b->free) {
			
			a->size = n->size + sizeof(SpaceNode);
			a->next = n->next;
			
		} else if (a->free && b->free) {
			
			a->size = n->size + b->size + 2*sizeof(SpaceNode);
			a->next = b->next;
			
		}
	}
			
	return 0;
}

void mydeallocate(void* ptr, char *file, int line, int request)
{
	switch (request) {
		case THREADREQ:
			// deallocate
			removeSpace(ptr, ELEMENT);
			break;
		default:
			// deallocate a page
			removeSpace(ptr, PAGE);

			
	}
}

int getPageSize(SpaceNode * ptr){
  //In case size doesn't have what I expect I can calculate the size I want later. For now I hope it's good.
  return ptr->size;

}

void memfun(int sig, siginfo_t *si, void *unused){
  //First, let's prevent any interrupts from being made.
  sigset_t oldmask;
  sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

  //check to make sure that memfun itself didn't raise a seg fault.
  if(inMemFun == 1){
    perror("memfun caused a segfault\n");
    exit(1);
  }
  inMemFun = 1;

  //Now check to make sure that the addr is actually within mem
  if(&mem[MEMORY_START] > si->si_addr || &mem[sizeof(mem) - 1] < si->si_addr){
    perror("Actual segmentation fault. Addr is outside of valid mem area.");
    exit(1);
  }

  //un-mprotect all the memory so we can mess with stuff. I'm not sure about the runtime of this so it may run slow, but it's easy to understand.
  mprotect(&mem[MEMORY_START], (&mem[sizeof(mem) - 1]), PROT_READ | PROT_WRITE);

  //Next, let's find out what page needs to be loaded.
  if(running == NULL){
    perror("running was null.\n");
    exit(1);
  }
  int currid = (*running)->id;
  if(mem[0] == 0){
    perror("Memory not initialized.\n");
    exit(1);
  }
  SpaceNode * pageptr = ((SpaceNode *)&mem[MEMORY_START]); //initializing a ptr to the beginning of mem
  int pgid = (int)(((char *)si->si_addr) - &mem[MEMORY_START]); //number of bytes from the beginning of mem
  pgid = pgid / PAGE_SIZE; //pgid now holds the inex of the expected page that user asked for.
  while(pageptr != NULL){
    if(pageptr->free == 0 && pageptr->pid == currid){
      if(pageptr->order <= pgid && pageptr->order + (getPageSize(pageptr) / PAGE_SIZE) >= pgid){
	printf("Found needed page.\n");
	break;
      }
    }
    pageptr = pageptr->next;
  } 
  //Now, let's move whatever is at the addresses of mem that pageptr needs to be at.

  SpaceNode * snptr = ((SpaceNode *)&mem[MEMORY_START]); //initializing another ptr
  SpaceNode * snprev;
  int pg = 0;
  while(pg <= pgid && snptr != NULL){
    pg += (getPageSize(snptr) / PAGE_SIZE) + 1;
    snprev = snptr;
    snptr = snptr->next;
  }
  int pagesneeded = (getPageSize(pageptr) / PAGE_SIZE) + 1;
  //if(snprev->free == 1){ //The page wants to be in free memory of unknown size.
  int snprev_pg_index = (int) ((((char *)snprev) - &mem[MEMORY_START])) / PAGE_SIZE;
  pagesneeded -= (pgid - snprev_pg_index) + 1;
  if(pagesneeded > 0 && snprev->next == NULL){
    printf("need more pages but we're at the end of mem...\n");
  }
  if(snprev->free == 0){
    SpaceNode * temp = (SpaceNode *)myallocate(getPageSize(snprev), __FILE__, __LINE__, -2);
    if(temp == NULL){
      perror("Couldn't find enough space to swapp.\n");
      exit(1);
    }
    memcpy(temp, snprev->start, snprev->size);
    temp--;
    temp->pid = snprev->pid;
    mydeallocate(snprev->start, __FILE__, __LINE__, -2);
  }
  //}
  while(pagesneeded > 0){ //start moving stuff stuff starting at snptr
    pagesneeded -= (getPageSize(snptr) / PAGE_SIZE) + 1;
    if(snptr->free == 1){
      snptr = snptr->next;
    }
    else {
      SpaceNode * temp = (SpaceNode *)myallocate(getPageSize(snptr), __FILE__, __LINE__, -2);
      if(temp == NULL){
	perror("Couldn't find enough space to swap.\n");
	exit(1);
      }
      memcpy(temp, snprev->start,snprev->size);
      temp--;
      temp->pid = snptr->pid;
      temp = snptr;
      snptr = snptr->next;
      mydeallocate(temp->start, __FILE__, __LINE__, -2);
    }
    if(snptr == NULL && pagesneeded > 0){ //change for swap file functionality!
      perror("Not enough room to shift stuff around.\n");
      exit(1);
    }
  }

  //Now we need to get the address where we'll be plopping the node down.
  SpaceNode * newloc = (SpaceNode *) (((int)&mem[MEMORY_START]) + (PAGE_SIZE * pgid));

  if (pagesneeded < 0){ //This means that the last page we removed was larger than we needed space for. Now we need to add in free space pagenode.
    printf("Running freeloc stuff.\n");
    SpaceNode * freeloc = (SpaceNode *) (((int)newloc) + sizeof(SpaceNode) + getPageSize(newloc));
    freeloc->start = freeloc + 1;
    if(snptr == NULL){
      freeloc->size = (PHYSICAL_SIZE + &mem[MEMORY_START]) - ((int)freeloc->start);
    } else {
      freeloc->size = ((int)snptr) - ((int)freeloc->start);
    }
    freeloc->free = 1;
    freeloc->next = snptr;
    freeloc->prev = newloc;
    newloc->next = freeloc;
  }

  //Will we be putting down memory in an arbitrary region of free space? possibly.
  if(newloc == snprev){
    //Great, where we want to put the page already lines up with the beginning of some free space.
  } else {
    //We need to do some pointer gynmnastics first.
    snprev->next = newloc;
    snprev->size = ((int)newloc) - ((int)snprev->start);
    newloc->prev = snprev;
  }
  memcpy((newloc + 1), (pageptr + 1), pageptr->size);
  newloc->pid = pageptr->pid;
  if(pagesneeded == 0){  //otherwise, newloc's next is already sorted out in a previous section
    newloc->next = snptr;
    if(snptr!= NULL){
      snptr->prev = newloc;
    }
  }

  mprotect(&mem[MEMORY_START],(int)(((char *)newloc) - &mem[MEMORY_START]), PROT_NONE);
  if(newloc->next != NULL){
    mprotect(newloc->next,(&mem[sizeof(mem) - 1]) - (int)newloc->next, PROT_READ | PROT_WRITE);
  }
  inMemFun = 1;
  //Still need to do memprotects and translation of addr.

  //Lastly, re-enable interrupts.
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

