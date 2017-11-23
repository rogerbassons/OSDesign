#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#define PAGE 0
#define ELEMENT 1

FILE *f = NULL;
int inHandler = 0;


typedef struct spaceNode {
	unsigned pid;
	unsigned reference;
	unsigned free;
	int order;
	void *start;
	unsigned size;
	struct spaceNode *next;
	struct spaceNode *prev;
	unsigned dataStartsAnotherPage;
        char swapclock;
} SpaceNode;

SpaceNode *getFirstPage()
{
	return (SpaceNode *) mem;
}

SpaceNode *getNextSpace(SpaceNode *old)
{
	if (old == NULL)
		return NULL;
	else
		return old->next;
}

SpaceNode *getLastPage()
{
	SpaceNode *n = getFirstPage();
	SpaceNode *prev = NULL;
	while (n != NULL) {
		prev = n;
		n = getNextSpace(n);
	}
	return prev;
}


void updateReference()
{
	SpaceNode *n = getFirstPage();
	while (n != NULL) {
		n->reference = 0;
		n = getNextSpace(n);
	}
}


SpaceNode *findFreePage()
{
	SpaceNode *n = getFirstPage();
	while (n != NULL && !(n->free)) 
		n = getNextSpace(n);

	return n;
	
}

SpaceNode *findFreeSpace(SpaceNode *n, size_t size)
{
	int found = 0;
	while (n != NULL && !found) {
		found = n->free && n->size >= size;
		if (!found)
			n = getNextSpace(n);
	}
	return n;
	
}

int initializeSpace(void *space, size_t size)
{

	
	SpaceNode new;

	new.free = 1;
	new.order = 1;
	new.next = new.prev = NULL;
	new.size = size - sizeof(SpaceNode);
	new.reference = 0;
	new.start = space + sizeof(SpaceNode);
	new.pid = 0;

	memcpy(space, &new, sizeof(SpaceNode));

	return 0;
}

int createSpace(SpaceNode *n, size_t size)
{
	if (!n->free) 
		return 1;


	n->free = 0;
	
	int restSize = n->size - size;
	n->size = size;
	if (restSize > sizeof(SpaceNode)) {
		SpaceNode new;
		new.free = 1;
		new.order = 1;
		new.prev = n;
		new.next = n->next;
		new.pid = 0;
		new.reference = 0;
		new.size = restSize;


		void *newPos = n->start + size;
		new.start = newPos + sizeof(SpaceNode);
		
		memcpy(newPos, &new, sizeof(SpaceNode));
		
		n->next = (SpaceNode *) newPos;
	}

		
		
	return 0;
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

	n->free = 1;
	n->pid = 0;

	if (type != PAGE) {
		if (a == NULL && b != NULL) {
		
			if (b->free) {
				n->size = b->size + sizeof(SpaceNode);
				n->next = b->next;
			}

		
		} else if (a != NULL && b == NULL) {
		
			if (a->free) {
				a->size = n->size + sizeof(SpaceNode);
				a->next = n->next;
			}		
			
		} else if (a != NULL && b != NULL) {
		
				
			if (!a->free && b->free) {
			
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
	}
		
			
	return 0;
}


void setProcessPage(SpaceNode *n, int pid)
{
	n->pid = pid;
}

void restorePointers(SpaceNode *n, size_t size, int type)
{
        if(size <= 0){
		printf("SpaceNode has size <= 0.\n");
		//This can happen at the very end of the swap array.
		//Not sure why the size ends up being zero.
		exit(1);
	}
	size_t s = 0;
	SpaceNode *old = NULL;
	while (s < size && size - s > sizeof(SpaceNode)) {
		size_t offset = sizeof(SpaceNode) + n->size;
		void *start = ((void *)n);
		
		SpaceNode *next = start + offset;
		n->next = next;
		n->prev = old;
		n->start = start + sizeof(SpaceNode);

		if (type == 0) {
			SpaceNode *firstElement = ((void *) n) + sizeof(SpaceNode);
			restorePointers(firstElement, n->size, 1);
		}
		

		old = n;
		n = next;
		s += offset;
	}
	old->next = NULL;
}

void restoreElementsPointers(SpaceNode *n, size_t size)
{
	
	restorePointers(n, size, 1);
}

void restorePagePointers(SpaceNode *n, size_t size)
{
	
	restorePointers(n, size, 0);
}

void fixSwapPtrs(char * swap)
{	
  void * ptr = swap + SWAP_MEMORY_START;
  SpaceNode * nodePtr = (SpaceNode *)swap;
  int s = 0;
  long max = swap + SWAP_MEMORY_START - sizeof(SpaceNode);
  long maxSize = SWAP_SIZE - (SWAP_MEMORY_START + pageSize );
  SpaceNode * old = NULL;
  while(nodePtr < max && s < maxSize){
    nodePtr->start = ptr;
    nodePtr->prev = old;
    if (old != NULL){
      old->next = nodePtr;}
    old = nodePtr;	  
    s+= pageSize;
    ptr += pageSize;
    nodePtr++; 
  }	
}

int getSwap(char* swap)
{
	// Create/Open Swap file
	f = fopen("swap", "r+b");
	if (f == NULL) {
		perror("Error creating swap file");
		return 1;
	}

	
	if (fread(swap, SWAP_SIZE, 1, f) != 1) {
		fprintf(stderr, "Error: Failed to read swap file\n");
		return 1;
	}
	//fflush(f);
	
	//restorePagePointers((SpaceNode *) swap, SWAP_SIZE);
	fixSwapPtrs(swap);
	fclose(f);
	return 0;
	
}


int writeSwap(char *swap)
{
        // printf("write checkswap:\n");
        //checkSwap(swap);
	//exit(1);
	// Create/Open Swap file
	f = fopen("swap", "w+b");
	if (f == NULL) {
		perror("Error creating/opening swap file");
		return 1;
	}
		

	if (fwrite(swap, SWAP_SIZE, 1, f) != 1) {// write swap array
		printf("Error writing to swap file");
		return 1;
	}
	fflush(f);
	return fclose(f);
}

SpaceNode *getVictimPage(unsigned pid) {
        if (SWAP_STRATEGY == 0){
	        SpaceNode *snptr = getLastPage();
		while(snptr != NULL && snptr->pid == pid){
		  snptr = snptr->prev;
		}
		return snptr;
	}
	SpaceNode *n = getLastPage();
	int rounds = 0;
	while(n != NULL && n->pid == pid && n->swapclock < 3){
	  n = n->prev;
	  n->swapclock++;
	  if(n == NULL && rounds < 2){
	    rounds++;
	    n = getLastPage();
	  }
	}
	return n;
}

int movePageToSwap(unsigned pid) //We avoid moving pages with pid to swapfile.
{	
        printf("Running movePageToSwap.\n");
	//exit(1);
	SpaceNode *evict = NULL;;


	evict = getVictimPage(pid);

	if(evict == NULL){
	  perror("Couldn't get a suitable page for eviction.\n");
	  return 1;
	}

	// READ SWAP FILE
	static char swap[SWAP_SIZE] = "";
	if(getSwap(swap) == 1){
		perror("Error from getswap.\n");
		return 1;
	}

	//SpaceNode *freeSwapPage = findFreeSpace((SpaceNode *)swap, evict->size);
	SpaceNode * freeSwapPage = (SpaceNode *)swap;
	while(freeSwapPage != NULL && freeSwapPage->free == 0){
	  freeSwapPage = freeSwapPage->next;
	}
	if (freeSwapPage == NULL) {
		fprintf(stderr, "Error: No free pages in swap file\n");
		return 1;
	}
	
	freeSwapPage->pid = evict->pid;
	freeSwapPage->free = 0;
	freeSwapPage->reference = evict->reference;
	freeSwapPage->dataStartsAnotherPage = evict->dataStartsAnotherPage;
	freeSwapPage->size = evict->size;
	freeSwapPage->order = evict->order;
	memcpy( freeSwapPage->start, evict->start, pageSize );
		        
	writeSwap(swap);

	removeSpace(evict, PAGE);
	 
	return 0;
}

// p1 and p2 are the same size
// swaps pages p1 and p2
int swapPages(SpaceNode *p1, SpaceNode *p2)
{
	if (p1 == p2)
		return 0;
	
	char buff[pageSize];
	//static char buff[4096];
	SpaceNode page1;
	SpaceNode page2;
	
	// Backup p1
	//int * tempGuy = (int *) ((SpaceNode *)(p1->start))->start;
	//printf("%d\n", *tempGuy);
 	memcpy(buff, p1->start, pageSize);
	memcpy(&page1, p1, sizeof(SpaceNode));

	//Backup p2
	memcpy(&page2, p2, sizeof(SpaceNode));


	// Copy p2  to p1
	memcpy(p1->start, p2->start, pageSize);
	memcpy(p1, p2, sizeof(SpaceNode));

	//Restore page pointers p1
	p1->start = page1.start;
	p1->next = page1.next;
	p1->prev = page1.prev;


	// Copy p1 backup to p2
	memcpy(p2->start, buff, pageSize);
	memcpy(p2, &page1, sizeof(SpaceNode));

	// Restore page pointers p2
	p2->start = page2.start;
	p2->next = page2.next;
	p2->prev = page2.prev;

	// restore pointers p1 and p2
	restoreElementsPointers(p1->start, p1->size);
	restoreElementsPointers(p2->start, p2->size);

	return 0;
}


void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreePage();
	
	
	if (n == NULL) {
		printf("No free pages, moving pages to swap...\n");
		if(USE_SWAP_F == 0){return NULL;}
		if (movePageToSwap(pid)) 
			return NULL;
		else 
			return getFreePage(size, pid);

	} else {
		
		n->free = 0;
		n->pid = pid;


		initializeSpace(n->start, n->size);
		
		if (VIRTUAL_MEMORY) {
			SpaceNode *first = getFirstPage();
			swapPages(first, n);

			return (void *)first;
		} else
			return (void *)n;
	}
}


SpaceNode *findPage(unsigned pid, int order)
{

	SpaceNode *n = getFirstPage();

	SpaceNode *res = NULL;
	while (n != NULL && res == NULL) {
		if (pid == n->pid && order == n->order)
			res = n;
		else
			n = getNextSpace(n);
	}
	/*
	if (res == NULL){
	  //printf("Running the problem bit!!\n");
	  static char swap[SWAP_SIZE];
	  if(getSwap(swap) == 1){
	    return NULL;
	  }
	  n = (SpaceNode *)swap;
	  while (n != NULL && res == NULL) {
	    if (pid == n->pid && order == n->order)
	      res = n;
	    else
	      n = n->next;
	  }
	  if(res != NULL){
	    successfulSwapFileChecks++;
	    SpaceNode *temp = getFreePage(pageSize, pid);
	    //temp->pid should be fine already
	    temp->reference = res->reference; //Not sure how critical this is
	    //temp->free should be fine already
	    temp->order = order;
	    //temp->start shouldn't change
	    temp->size = res->size;
	    //temp->next and temp->prev shouldn't change
	    temp->dataStartsAnotherPage = res->dataStartsAnotherPage;
	    memcpy(temp->start, res->start, res->size);
	    removeSpace(res, PAGE); //Not sure about removeSpace working with a pointer to swap, but it oughtta be ok 
	    //res->pid = 0;
	    //res->free = 1;
	    writeSwap(swap);
	    res = temp;
	  }
	  //printf("About to exit the problem bit. Res was: %lu\n", res);
	}
	*/
	if(res != NULL){
	  res->swapclock = 0;
	}
	return res;
}


SpaceNode *findPageInSwap(unsigned pid, int order)  //Finds a page in swap file and places it in main memory.
{
  static char swap[SWAP_SIZE];
  if(getSwap(swap) == 1){
    return NULL;
  }
  SpaceNode *n = (SpaceNode *)swap;
  SpaceNode *res = NULL;
  while (n != NULL && res == NULL) {
    if (pid == n->pid && order == n->order && n->free == 0)
      res = n;
    else
      n = n->next;
  }
  
  if(res != NULL){
    SpaceNode *temp = getFreePage(pageSize, pid);
    //temp->pid should be fine already
    temp->reference = res->reference; //Not sure how critical this is
    //temp->free should be fine already
    temp->order = order;
    //temp->start shouldn't change
    temp->size = res->size;
    //temp->next and temp->prev shouldn't change
    temp->dataStartsAnotherPage = res->dataStartsAnotherPage;
    memcpy(temp->start, res->start, res->size);
    removeSpace(res, PAGE); //Not sure about removeSpace working with a pointer to swap, but it oughtta be ok 
    //res->pid = 0;
    //res->free = 1;
    writeSwap(swap);
    res = temp;
    res->swapclock = 0;
  }
  //printf("About to exit the problem bit. Res was: %lu\n", res);	
  return res;
}


SpaceNode *findProcessPage(unsigned pid)
{
	return findPage(pid, 1);
}

void *getFreeElementFrom(void *memory, size_t size)
{
	SpaceNode *n = findFreeSpace((SpaceNode *)memory, size);

	if (n == NULL) {
		perror("Error: No free space for OS data");
		return NULL;
	} else {
		
		if (createSpace(n, size)) {
			perror("Error creating OS space");
			return NULL;
		}

		return (void *)n->start;
	}
}

int memoryProtect(unsigned pid)
{

	SpaceNode *p = findPage(pid, 1);

	int i = 2;
	while (p != NULL) {
			if(mprotect(p->start, pageSize, PROT_NONE))
				return 1;
		p = findPage(pid, i);
		i++;
	}

	return 0;
}

int memoryAllowAll()
{
	if(mprotect(mem + MEMORY_START, PHYSICAL_SIZE - MEMORY_START - pageSize * 4, PROT_READ | PROT_WRITE))
		return 1;
	return 0;
}

int memoryAllow(unsigned pid)
{

	int i = 1;
	SpaceNode *p = findPage(pid, i);
	i++;
	while (p != NULL) {
		if(mprotect(p->start, pageSize, PROT_READ | PROT_WRITE))
			return 1;
		p = findPage(pid, i);
		i++;
	}
	
	return 0;
}

void printMemory()
{
	
	
	SpaceNode *n = getFirstPage();

	printf("\n\n\nMemory: \n----------------------------------\n");

	while (n != NULL) {
		if (!n->free) {
			printf("----- Page thread %i -----\n", n->pid);
			printf("Page start address %p     \n", n->start);
			printf("Order %i     \n", n->order);
			printf("Size: %i\n", n->size);
			
			//memoryAllow(n->pid);
			if (!(n->dataStartsAnotherPage == 1)) {
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
				//memoryProtect(n->pid);
				
			} else
				printf("***This page's content starts on previous process page***\n");
			printf("----------------------------------\n");
		}
		n = getNextSpace(n);

	}
	printf("----------------------------------\n\n\n");
	
}

SpaceNode *getContiguousPage(SpaceNode *page)
{
	SpaceNode *contiguousPage = NULL;

	SpaceNode *n = getFirstPage();
	while (n != NULL && contiguousPage == NULL) {
		if (n->start == page->start + pageSize)
			contiguousPage = n;
		n = getNextSpace(n);
	}
	return contiguousPage;
}

// page1 is page1 + page2
void makeContiguous(SpaceNode *page1, SpaceNode *page2)
{
	if (page1 != page2) {
		SpaceNode *contiguousPage = getContiguousPage(page1);
		swapPages(contiguousPage, page2);
	}
	      
}

int loadProcessPages(unsigned pid) {

        SpaceNode *p1 = findPage(pid, 1);
	if(p1 == NULL && USE_FIND_IN_SWAP){ p1 = findPageInSwap(pid, 1); }
	swapPages(getFirstPage(), p1);
	p1 = getFirstPage();
	
	SpaceNode *p2 = findPage(pid, 2);
	if(p2 == NULL && USE_FIND_IN_SWAP){ p2 = findPageInSwap(pid, 2); }

	unsigned i = 3;
	while (p1 != NULL && p2 != NULL) {
		makeContiguous(p1, p2);

		p1 = p2;
		p2 = findPage(pid, i);
		if(p2 == NULL && USE_FIND_IN_SWAP){ p2 = findPageInSwap(pid, i); }
		i++;
	}


	return 0;
}

SpaceNode *getLastElement(SpaceNode *page)
{
	SpaceNode *last = page->start;
	SpaceNode *e = getNextSpace(last);

	while (e != NULL) {
		last = e;
		e = getNextSpace(e);
	}

	return last;
}

SpaceNode *getLastProcessPage(unsigned pid)
{
	SpaceNode *last = findPage(pid, 1);
	SpaceNode *p = findPage(pid, 2);

	int i = 3;
	while (p != NULL) {
		last = p;
		p = findPage(pid, i);
		i++;
	}

	return last;
}

void joinFreeSpace(SpaceNode *newPage, unsigned pid)
{
	SpaceNode *prev = findPage(pid, 1);
	SpaceNode *p = findPage(pid, 2);
	int i = 3;
	while (p != NULL) {
		prev = p;
		p = findPage(pid, i);
		i++;
	}
	p = prev; // last page

	i = prev->order - 1;
	while (p != NULL && p->dataStartsAnotherPage) {
		prev = p;
		p = findPage(pid, i);
		i--;
	}

	if (prev != NULL && !prev->dataStartsAnotherPage) {
			SpaceNode *e = getLastElement(p);
			if (e != NULL && e->free) {
				newPage->dataStartsAnotherPage = 1;
				e->size += newPage->size;
			}
	}
	

}

int reserveAnotherPage(unsigned pid)
{


	SpaceNode *newPage = getFreePage(pageSize, pid);
	if (newPage == NULL) {
		printf("No free pages while reserving another page\n");
		return 1;
	}


	
	newPage->order = -1;
	joinFreeSpace(newPage, pid);

	SpaceNode *last = getLastProcessPage(pid);
	newPage->order = last->order + 1;
	

	loadProcessPages(pid);
	
	return 0;
}



void *getFreeElement(size_t size)
{

	unsigned pid = 1;
	if (running != NULL)
		pid = (*running)->id;


	
	SpaceNode * p = NULL;
	if (VIRTUAL_MEMORY) {
		loadProcessPages(pid);
		p = getFirstPage();
	} else {
		p = findPage(pid, 1);
	}
	if (p == NULL) {
		perror("Cannot find process page");
		return NULL;
	}
	

	p->reference = 1;
	SpaceNode *n = findFreeSpace((SpaceNode *)(p->start), size);

	
	
	if (n == NULL) {
		if (!VIRTUAL_MEMORY) {
			return NULL;
		} else {
			if (size > PHYSICAL_SIZE - MEMORY_START)
				printf("Unable to reserve bigger than physical memory size\n");
		
			p = findPage(pid, 2);
			int i = 3;
			while (p != NULL && n == NULL) {
			
				n = findFreeSpace((SpaceNode *)(p->start), size);
				if (i == -1) {
					//printMemory();
					//exit(1);
				}
				p = findPage(pid, i);
				i++;
			
			
			}

			if (n == NULL) {
				if (reserveAnotherPage(pid)) {
					perror("Error reserving another page: no free space");
					return NULL;
				}
				return getFreeElement(size);
			}
		} 
	}

		
	if (createSpace(n, size)) {
		perror("Error creating space");
		return NULL;
	}


	return (void *)n->start;
}

void printOSMemory()
{
	printf("\n\nOS Memory: \n----------------------------------\n");
	SpaceNode *n = (SpaceNode *) (mem + OSRESERVED_START);
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



void initializeFreePages(void *start, size_t freeSpace)
{
	SpaceNode i;
	i.free = 1;
	i.reference = 0;
	i.order = 1;
	i.next = i.prev = NULL;
	i.size = pageSize - sizeof(SpaceNode);
	i.pid = 0;

	SpaceNode *prev = NULL;

	while (freeSpace >= pageSize) {
		i.start = start + sizeof(SpaceNode);
		i.prev = prev;

		memcpy(start, &i, sizeof(SpaceNode));

		if (prev != NULL) {
			prev->next = (SpaceNode *)start;
		}

		prev = (SpaceNode *)start;
		freeSpace = freeSpace - pageSize;
		
		start += pageSize;
	}
}


static void handler(int sig, siginfo_t *si, void *unused)
{
	//printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
        if(inHandler == 1){
	  perror("The handler segfaulted somewhere.\n");
	  exit(1);
	}
	inHandler = 1;

	long addr = (long) si->si_addr;

	long first = (long) mem + MEMORY_START;
	long last = (long) sharedMemoryStart;
	
	if (addr >= first && addr < last) {


		unsigned pid = (*running)->id;
		memoryAllowAll();

		loadProcessPages(pid);

		
	} else {
		sigaction(SIGSEGV, &oldSIGSEGV, NULL);
	}
	inHandler = 0;
}

void setSIGSEGV()
{
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	
	if (sigaction(SIGSEGV, &sa, &oldSIGSEGV) == -1) {
			
		printf("Fatal error setting up signal handler\n");
		exit(EXIT_FAILURE);    //explode!
	}
}

void init()
{
	if (posix_memalign((void **)&mem, sysconf(_SC_PAGE_SIZE), PHYSICAL_SIZE)) {
		perror("Can't reserve space for main memory");
		exit(1);
	}

	pageSize = sysconf( _SC_PAGE_SIZE);

	// Last 4 pages are shared
	size_t sharedSize = (pageSize * 4);
	sharedMemoryStart = mem + PHYSICAL_SIZE - sharedSize;
	initializeFreePages(sharedMemoryStart, sharedSize);
		
	// System reserved space
	SpaceNode new;
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = MEMORY_START - OSRESERVED_START - sizeof(SpaceNode);
	new.start = mem + OSRESERVED_START + sizeof(SpaceNode);
	new.pid = 0;
	new.order = 1;
	new.swapclock = 0;
	memcpy(mem + OSRESERVED_START, &new, sizeof(SpaceNode));
	// Free space for pages
	// Pages bookkeeping
	
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = pageSize;
	new.pid = 0;
	new.order = 1;
	new.dataStartsAnotherPage = 0;
	
	void *ptr = mem + MEMORY_START;
	void *nodePtr = mem;
	

	size_t s = 0;
	void *max = mem + OSRESERVED_START - sizeof(SpaceNode);
	size_t maxSize = PHYSICAL_SIZE - sharedSize - MEMORY_START;
	SpaceNode* old = NULL;
	while (nodePtr < max && s < maxSize) {
		new.start = ptr;
		new.prev = old;
		if (old != NULL)
			old->next = nodePtr;
		old = nodePtr;

		memcpy(nodePtr, &new, sizeof(SpaceNode));
		
		s+= pageSize;
		ptr += pageSize;
		nodePtr += sizeof(SpaceNode);
	}

	// set SIGSEGV handler
	if (VIRTUAL_MEMORY)
		setSIGSEGV();

	// Create Swap file
	static char swap[SWAP_SIZE] = "";
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = pageSize;
	new.pid = 0;
	new.order = 0;
	new.swapclock = 0;
	new.dataStartsAnotherPage = 0;
	ptr = swap + SWAP_MEMORY_START;
	nodePtr = swap;
	s = 0;
	max = swap + SWAP_MEMORY_START - sizeof(SpaceNode);
	maxSize = SWAP_SIZE - (SWAP_MEMORY_START + pageSize );
	old = NULL;
	while(nodePtr < max && s < maxSize){
	  new.start = ptr;
	  new.prev = old;
	  if (old != NULL)
	    old->next = nodePtr;
	  old = nodePtr;
	  
	  memcpy(nodePtr, &new, sizeof(SpaceNode));
	  
	  s+= pageSize;
	  ptr += pageSize;
	  nodePtr += sizeof(SpaceNode); 
	}
	
	writeSwap(swap);
	//printSwapF(10);
	//exit(1);
}

void *myallocate (size_t size, char *file, int line, int request)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa->sa_mask, &oldmask);
	

	if (mem == NULL) { //first run
		init();
	}

	memoryAllowAll();

	void *ptr = NULL;
	switch (request) {
	case SHALLOC:
		// allocate inside a thread page
		ptr = getFreeElementFrom(sharedMemoryStart, size);
		break;
	case THREADREQ:
		// allocate inside a thread page
		if (running == NULL) {
			initOS();
		}
		ptr = getFreeElement(size);
		break;
	case OSREQ:
		// allocate memory to OS data
		ptr = getFreeElementFrom(mem + OSRESERVED_START, size);	
		break;
	default:
		// reserve a page to a thread with id request
		ptr = getFreePage(pageSize, request);
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	
	return ptr;
}

void mydeallocate(void* ptr, char *file, int line, int request)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa->sa_mask, &oldmask);

	switch (request) {
	case THREADREQ:
		// deallocate
		removeSpace(ptr, ELEMENT);
		break;
	case OSREQ:
		// deallocate memory from OS data
		removeSpace(ptr, ELEMENT);
		break;
	default:
		// deallocate a page
		removeSpace(ptr, PAGE);

	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}


void *shalloc (size_t size) {
	return myallocate(size, "shalloc", 0, SHALLOC);
}

void printSwapF(int k){
  static char swap[SWAP_SIZE];
  if(getSwap(swap)==1){
    printf("Error opening swap.\n");
    return;
  }
  int val = 0;
  SpaceNode * n = (SpaceNode *)&swap[0];
  printf("Printing first %d pages of swapfile.\n", k, n);
 
  int i = 0;
  for(i = 0; i < k; i++){
    if(n == NULL){
      printf("printSwapF finished early: node was null.\n");
      return;
    }
    val = *(int *)( ((SpaceNode *)(n->start)) + 1 );
    printf("%dth Node. n is at %d. Pid is %d. Order is %d. Next is at %d, val was %d.\n", i, n, n->pid, n->order, n->next, val);
    n = n->next;
  }
}


