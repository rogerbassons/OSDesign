#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#define PAGE 0
#define ELEMENT 1

FILE *f = NULL;
int shared_init = 0;
int threadOrders[1000] = {0}; //Can have at most 1000 threads
int PAGE_SIZE;
int inMemFun = 0;

typedef struct spaceNode {
	unsigned pid;
	unsigned reference;
	unsigned free;
	unsigned order;
	unsigned split;
	void *start;
	unsigned size;
	unsigned unusedSize;
	struct spaceNode *next;
	struct spaceNode *prev;
} SpaceNode;


SpaceNode *getFirstPage()
{
	return (SpaceNode *) (mem + MEMORY_START);
}

SpaceNode *getLastPage()
{
	return (SpaceNode *) (sharedMemoryStart - pageSize);
}


SpaceNode *getNextSpace(SpaceNode *old)
{
	if (old == NULL)
		return NULL;
	else
		return old->next;
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

int initializeSpace(SpaceNode *n)
{

	
	SpaceNode new;

	new.free = 1;
	new.split = 0;
	new.order = 1;
	new.next = new.prev = NULL;
	new.size = n->size - sizeof(SpaceNode);
	new.unusedSize = 0;
	new.reference = 0;
	new.start = n->start + sizeof(SpaceNode);
	new.pid = 0;

	memcpy(n->start, &new, sizeof(SpaceNode));

	return 0;
}

int createSpace(SpaceNode *n, size_t size, int type)
{
	if (!n->free || ((type != PAGE) && n->size < size)) 
		return 1;


	n->free = 0;
	
	if (type != PAGE) {
		int restSize = n->size - size;
		n->size = size;
		if (restSize > sizeof(SpaceNode)) {
			
			SpaceNode new;
			new.free = 1;
			new.split = 0;
			new.order = 1;
			new.prev = n;
			new.next = n->next;
			new.pid = 0;
			new.unusedSize = 0;
			new.reference = 0;
			new.size = restSize;


			void *newPos = n->start + size;
			new.start = newPos + sizeof(SpaceNode);
		
			memcpy(newPos, &new, sizeof(SpaceNode));
		
			n->next = (SpaceNode *) newPos;
		} else if (restSize > 0) {
			SpaceNode *p = n->prev;
			SpaceNode *page = NULL;
			while (p != NULL) {
				page = p;
				p = p->prev;
			}
			if (page != NULL) {
				page = ((void *) page) - sizeof(SpaceNode);
				page->unusedSize += restSize;
			}
		}

		
	} else
		initializeSpace(n); // create a free node inside the page

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
	n->split = 0;

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
		//printf("SpaceNode has size <= 0.\n");
		//This can happen at the very end of the swap array. Not sure why the size ends up being zero.
		return;
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
			restorePointers(firstElement, n->size - n->unusedSize, 1);
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
	
	restorePagePointers((SpaceNode *) &swap[0], SWAP_SIZE);
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

SpaceNode *getVictimPage() {
	if (SWAP_STRATEGY == 0)
		return getLastPage();
	
	SpaceNode *n = getFirstPage();
	while (n != NULL) {
		if (n->size == pageSize && n->reference == 0)
			return n;

		n = getNextSpace(n);
	}
	return n;
}

int movePageToSwap()
{	
	
	SpaceNode *evict = NULL;;


	evict = getVictimPage();

	// READ SWAP FILE
	static char swap[SWAP_SIZE];
	if(getSwap(swap) == 1){
		printf("Error from getswap.\n");
	}

	SpaceNode *freeSwapPage = findFreeSpace((SpaceNode *)swap, pageSize);
	if (freeSwapPage == NULL) {
		fprintf(stderr, "Error: No free pages in swap file\n");
		return 1;
	}

	memcpy(freeSwapPage, evict, pageSize);
		        
	writeSwap(swap);

	removeSpace(evict, PAGE);
	 
	return 0;
}

// p1 and p2 are the same size
// swaps pages p1 and p2
int swapPages(SpaceNode *p1, SpaceNode *p2)
{
	size_t p1size = p1->size + p1->unusedSize;
	size_t p2size = p2->size + p2->unusedSize;
	if (p1 == p2 || p1size != p2size) {
		return 1;
	}
	char copy[pageSize];

	void *start = p1->start;
	SpaceNode *next = p1->next;
	SpaceNode *prev = p1->prev;


	memcpy((char *) copy, (char *) p1, pageSize);

	memmove((void *) p1, (void *) p2, pageSize);
	p1->next = next;
	p1->prev = prev;
	p1->start = start;
	restoreElementsPointers(p1->start, p1size);

	start = p2->start;
	next = p2->next;
	prev = p2->prev;

	memmove((void *) p2, (void *) copy, p2size + sizeof(SpaceNode));
	p2->next = next;
	p2->prev = prev;
	p2->start = start;
	restoreElementsPointers(p2->start, p2size);


	return 0;
}


void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreePage();
	
	
	if (n == NULL) {
		printf("No free pages, moving pages to swap...\n"); //TODO DEVEL
		if (movePageToSwap(size)) 
			return NULL;
		else 
			return getFreePage(size, pid);

	} else {
		
		if (createSpace(n, size, PAGE)) {
			fprintf(stderr, "Error creating space for process %i\n", pid);
			return NULL;
		}
			
		setProcessPage(n, pid);

		if (VIRTUAL_MEMORY) {
			SpaceNode *first = getFirstPage();
			swapPages(first, n);

			return (void *)first;
		} else
			return (void *)n;
	}
}


SpaceNode *findSplitPage(unsigned pid, int number)
{

	SpaceNode *n = getFirstPage();


	int found = 0;
	while (n != NULL && !found) {

		found = n->pid == pid && n->order == number;
		if (!found)
			n = getNextSpace(n);
			
	}
	return n;
}


SpaceNode *findPage(unsigned pid, SpaceNode *start, int number)
{

	SpaceNode *n = getFirstPage();
	if (start != NULL)
		n = start;

	int i = 0;
	SpaceNode *res = NULL;
	while (n != NULL && i < number) {
	
		while (n != NULL && n->pid != pid) {
			n = getNextSpace(n);
		}
		
		if (n != NULL && n->pid == pid) {
			res = n;
			n = getNextSpace(n);
			i++;
		} 
			
	}
	return res;
}

SpaceNode *findProcessPage(unsigned pid, SpaceNode *start)
{
	return findPage(pid, start, 1);
}

void *getFreeElementFrom(void *memory, size_t size)
{
	SpaceNode *n = findFreeSpace((SpaceNode *)memory, size);

	if (n == NULL) {
		perror("Error: No free space for OS data");
		return NULL;
	} else {
		
		if (createSpace(n, size, ELEMENT)) {
			perror("Error creating OS space");
			return NULL;
		}

		return (void *)n->start;
	}
}

void joinFreeMemory(SpaceNode *e)
{
	SpaceNode *last = e;
	e = e->next;

	while (e != NULL) {
		if (last->free && e->free) {
			last->size = last->size + e->size + sizeof(SpaceNode);
			last->next = e->next;
		}

		e = e->next;
	}
}

int printData(void * start, int type)
{
	SpaceNode *n = (SpaceNode *)start;
	if (type == 0) {
		printf("Memory: \n----------------------------------\n");
	} else {
		printf("Swap: \n----------------------------------\n");
	}
	while (n != NULL) {
		if (!n->free) {
			printf("----- Page thread %i -----\n", n->pid);
			printf("Page address %p     \n", n);
			printf("Size: %i\n", n->size);
			printf("Unused size: %i\n", n->unusedSize);

			if (!n->split) {
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
			} else
				printf("      Page is split number %i\n", n->order);
		     
			
			printf("\n");
		}
		n = getNextSpace(n);	


	}
	printf("----------------------------------\n\n\n");
	return 0;
}

// page1 is page1 + page2
void makeContiguous(SpaceNode *page1, SpaceNode *page2)
{
	if (page1 != page2) {
		SpaceNode *contiguousPage = page1->next;

		swapPages(contiguousPage, page2);


		void *dataStart = ((void *)page1) + sizeof(SpaceNode) + page1->size;
		page1->split = 0;
		page1->size += page2->size;
		page1->unusedSize += sizeof(SpaceNode);
		page1->next = page2->next;


		memmove(dataStart, page2->start, page2->size);
		
		restoreElementsPointers(page1->start, page1->size - page1->unusedSize);
		joinFreeMemory(page1->start);
	}
	      
}

// splits first page into system page sized pages
void splitPages()
{

	SpaceNode *p = getFirstPage();
	
	size_t size = p->size + p->unusedSize;


	if (size > pageSize) {
		void *pages = getFirstPage();
		char copy[size - p->unusedSize];
		memcpy(copy, p, size - p->unusedSize); // copy multiple pages "page" to copy

		SpaceNode new;
		new.free = 0;

		new.split = 1;
		new.pid = p->pid;
		new.size = pageSize - sizeof(SpaceNode);
		new.unusedSize = 0;
		new.next = NULL;
		new.order = 0;
		
	
		void *start = pages;
		void *copyStart = copy + sizeof(SpaceNode);
		SpaceNode *prev = NULL;

		while (size >= pageSize) {

			if (prev != NULL)
				prev->next = start;
			new.prev = prev;
		
			new.start =  start + sizeof(SpaceNode);

			new.order += 1;
			memcpy(start, &new, sizeof(SpaceNode));
			prev = (SpaceNode *) start;
			start += sizeof(SpaceNode);

		
			memmove(start, copyStart, new.size);
			start += new.size;
			copyStart +=  new.size;


			size -= new.size;
		}
		if (prev != NULL)
			prev->next = start;
	}
}


int reserveAnotherPage(SpaceNode *page)
{
	unsigned pid = page->pid;
	SpaceNode *newPage = getFreePage(sysconf( _SC_PAGE_SIZE), pid);
	if (newPage == NULL) {
		printf("No free pages while reserving another page\n");
		return 1;
	}
	newPage->order = page->order += 1;

	SpaceNode *first = findPage(pid, NULL, 1);
	SpaceNode *second = findPage(pid, NULL, 2);
	makeContiguous(first, second);
	return 0;
}



void *tryGetFreeElement(size_t size, int try)
{
	unsigned pid = 1;
	if (running != NULL)
		pid = (*running)->id;

	SpaceNode * p = findProcessPage(pid, NULL);
	if (p == NULL) {
		perror("Cannot find process page");
		return NULL;
	}
	p->reference = 1;
		
	if (VIRTUAL_MEMORY) {
		swapPages(getFirstPage(), p);
		p = getFirstPage();
	}

	size_t findSize = size;
	if (try > 0)
		findSize += sizeof(SpaceNode);
	SpaceNode *n = findFreeSpace((SpaceNode *)(p->start), findSize);

	if (n == NULL) {
		if (VIRTUAL_MEMORY) {
			if (size > PHYSICAL_SIZE - MEMORY_START)
				printf("Unable to reserve bigger than physical memory size\n");
			if (reserveAnotherPage(p)) {
				perror("Error reserving another page: no free space");
				return NULL;
			}
			return tryGetFreeElement(size,1);
			
		}
		return NULL;
			
	} else {

		
		if (createSpace(n, size, ELEMENT)) {
			perror("Error creating space");
			return NULL;
		}
		return (void *)n->start;
	}
}

void *getFreeElement(size_t size) {
	if (running == NULL) {
		initOS();
	}
	return tryGetFreeElement(size, 0);
}

void printOSMemory()
{
	printf("\n\nOS Memory: \n----------------------------------\n");
	SpaceNode *n = (SpaceNode *) mem;
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




void printSwap()
{
        static char swap[SWAP_SIZE];
	if(getSwap(swap) == 1){
		printf("Printswap: error from getswap.\n");
	}
	printData(swap, 1);
}

void printMemory()
{
	printData(getFirstPage(), 0);
}

void initializeFreePages(void *start, size_t freeSpace)
{
	SpaceNode i;
	i.free = 1;
	i.reference = 0;
	i.unusedSize = 0;
	i.order = 1;
	i.split = 0;
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

int memoryProtect(void *page)
{
	if(mprotect(page, sysconf(_SC_PAGE_SIZE), PROT_NONE))
		return 1;
	return 0;
}

int memoryAllow()
{
	mprotect(mem + MEMORY_START, pageSize, PROT_READ | PROT_WRITE);
	SpaceNode *first = getFirstPage();
	if (first->size > pageSize - sizeof(SpaceNode)) {
		mprotect(mem + MEMORY_START, first->size, PROT_READ | PROT_WRITE);
	}
	return 0;
}

void loadRunningProcessPages() {

	unsigned pid = (*running)->id;

	SpaceNode *p = findProcessPage(pid, NULL);
	if (p != NULL) {


		if (p->split) {

			p = findSplitPage(pid, 1);
			swapPages(getFirstPage(), p);

			p = findSplitPage(pid, 2);

			int i = 3;
			while (p != NULL) {

				makeContiguous(getFirstPage(), p);
				p = findSplitPage(pid, i);
				i++;
			}
		} else
			swapPages(getFirstPage(), p);

		getFirstPage()->reference = 1;
	}
}


static void handler(int sig, siginfo_t *si, void *unused)
{
	//printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);

	long addr = (long) si->si_addr;

	long first = (long) mem + MEMORY_START;
	long last = (long) sharedMemoryStart;
	
	if (addr >= first && addr < last) {

		memoryAllow();
		splitPages();

		loadRunningProcessPages();

		
	} else {
		sigaction(SIGSEGV, &oldSIGSEGV, NULL);
	}
}

void setSIGSEGV()
{
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	//sa.sa_sigaction = handler;
	sa.sa_sigaction = memfun;	
	
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
	new.size = MEMORY_START - sizeof(SpaceNode) - 1;
	new.start = mem + sizeof(SpaceNode);
	new.pid = 0;
	new.order = 1;

	memcpy(mem, &new, sizeof(SpaceNode));

	// Free space for pages
	initializeFreePages(mem + MEMORY_START, PHYSICAL_SIZE - sharedSize - MEMORY_START);

	// set SIGSEGV handler
	if (VIRTUAL_MEMORY || ANDREW_VM)
		setSIGSEGV();

	// Create Swap file
	static char swap[SWAP_SIZE] = "";
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = SWAP_SIZE - sizeof(SpaceNode);
	new.start = swap + sizeof(SpaceNode);
	new.pid = 0;
	memcpy(swap, &new, sizeof(SpaceNode));
	initializeFreePages(swap, SWAP_SIZE);
	writeSwap(swap);

}

void *myallocate (size_t size, char *file, int line, int request)
{
	sigset_t oldmask;
	if(request != VMREQ){
	  sigprocmask(SIG_BLOCK, &sa->sa_mask, &oldmask);
	}

	if (mem == NULL) { //first run
		init();
		PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
	}

	void *ptr = NULL;
	switch (request) {
	case SHALLOC:
		// allocate inside a thread page
		ptr = getFreeElementFrom(sharedMemoryStart, size);
		break;
	case THREADREQ:
	  //printf("Myallocate: case threadreq.\n");
		// allocate inside a thread page
		if(ANDREW_VM == 1){
		  mprotect(&mem[MEMORY_START], PHYSICAL_SIZE - MEMORY_START, PROT_READ | PROT_WRITE);
		}
		ptr = getFreeElement(size);
		if(ANDREW_VM == 1){
		  //printf("&mem[Memorystart]: %p\n", (void *)&mem[MEMORY_START]);
		  ptr = translatePtr(ptr);
		  mprotect(&mem[MEMORY_START], PHYSICAL_SIZE - MEMORY_START, PROT_NONE);
		}
		break;
	case OSREQ:
		// allocate memory to OS data
		ptr = getFreeElementFrom(mem, size);
		break;
	default:
	  //printf("Myallocate: case default.\n");
		// reserve a page to a thread with id request
		ptr = getFreePage(size, request);

	}
	if(request != VMREQ){
	  sigprocmask(SIG_SETMASK, &oldmask, NULL);
	}
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


void *shalloc (size_t size){
	return myallocate(size, "shalloc", 0, SHALLOC);
}


/*
  translateptr is a function used to transform the input real memory address to a virtual one.
  
  To do this, an array called threadOrders exist to see how many pages some thread has created.
  Each element in threadOrders should increment by however many pages a call to malloc requested.
  You dereference threadOrders by the current thread's pid.
  Updating this value should probably happen when you update the pid of a page's SpaceNode.
  Sometimes, a malloc call can give a thread some space in a page it already allocated, which
  should not increment threadOrders[running->pid].
  
  In my incomplete implementation below, I assumed each malloc call would require a new page,
  and so update threadOrders[running->pid] here instead of elsewhere in myallocate.
  
  threadOrders[running->pid] serves as a sort of index that describes which real page the virtual
  address should point to.
  
  To translate, first we need to find what real page the real address corresponds to. Since the real
  page could be of any size (as long as it's a multiple of sysconf(_SC_PAGE_SIZE) ), we have to go
  from the beginning of mem and check to see if the real addr is contained in each page's address space.
  Once we have the right page, we need to calculate the offset between the page's address and the real address.
  Then we calculate the virtual address based on the 'index' from threadOrders and the offset.
  
  There is one more issue I haven't considered yet: For each thread, the virtual addresses are always
  strictly increasing. That means that if one thread tries to allocate more than the maximum amount
  of memory, its entry in threadOrders will correspond to a page outside of memory. Since all the
  information we have about what page needs to be swapped in is an address and the current running
  thread, I don't think any implementation will fix this (If you loop the index, then you might
  have collisions where one virtual address could correspond to two pages). However, the user
  could free an earlier page, and the implementation as I've described would not be able to find
  that free virtual page.
  
*/
void *translatePtr(void * ptr){
  if((long)ptr > (long)(&mem[PHYSICAL_SIZE - 1])){
    perror("ptr is out of range of mem.\n");
    exit(1);
  }
  SpaceNode * snptr = (SpaceNode *) &mem[MEMORY_START];
  while((long)snptr < (long)ptr){
    if(snptr->next == NULL){ //could be the very last page
      break;
    }
    if((long)snptr->next > (long)ptr){
      break;
    }
    snptr = snptr->next;
  }
  
  //Now, snptr contains the page in actual memory where ptr is pointing to. From this, we can calculate an offset from snptr.
  
  if(threadOrders[snptr->pid] == 0){ //malloc seems to start assigning from one page off? This should compensate.
    threadOrders[snptr->pid]++;
  }

  long snoffset = ((long)ptr) - ((long)snptr);
  snptr->order = threadOrders[snptr->pid];
  threadOrders[snptr->pid] += ((snptr->size + sizeof(SpaceNode) - 1) / PAGE_SIZE) + 1;
  void * virloc = (void *) ((long)&mem[MEMORY_START] + (PAGE_SIZE * snptr->order) + snoffset);
  return virloc;
}






int getPageSize(SpaceNode * ptr){
  //In case size doesn't have what I expect I can calculate the size I want later. For now I hope it's good.
  return ptr->size;
  
}


void memfun(int sig, siginfo_t *si, void *unused){
  //First, let's prevent any alarm interrupts from being made. This shouldn't affect seg fault interrupts.
  sigset_t oldmask;
  sigprocmask(SIG_BLOCK, &sa->sa_mask, &oldmask);
  
  //check to make sure that memfun itself didn't raise a seg fault.
  if(inMemFun == 1){
    perror("memfun caused a segfault\n");
    exit(1);
  }
  inMemFun = 1;
  
  //Now check to make sure that the addr is actually within mem
  if((long)&mem[MEMORY_START] > (long)si->si_addr || (long)&mem[PHYSICAL_SIZE - 1] < (long)si->si_addr){
    perror("Actual segmentation fault. Addr is outside of valid mem area.\n");
    exit(1);
  }
  
  //un-mprotect all the memory so we can mess with stuff. I'm not sure about the runtime of this so it may run slow, but it's easy to understand.
  //NOTE: mprotect needs to start at a location that is system-page-aligned. I figure the easiest way to do this is to move MEMORY_START
  //so that &mem[MEMORY_START] % sysconf(_SC_PAGE_SIZE) = 0. Note that I have not done this yet since I don't know how important MEMORY_START
  //staying the same is to your code.
  mprotect(&mem[MEMORY_START], PHYSICAL_SIZE - MEMORY_START, PROT_READ | PROT_WRITE);
  
  //Next, let's find out what real page is sitting at the virtual address's space.
  if(running == NULL){ //sanity check
    perror("running was null.\n");
    exit(1);
  }
  int currid = (*running)->id;
  
  SpaceNode * pageptr = ((SpaceNode *)&mem[MEMORY_START]);
  long pgid = (long)si->si_addr - (long)&mem[MEMORY_START]; //number of bytes from the beginning of mem
  pgid = pgid / PAGE_SIZE; //pgid now holds the inex of the expected page that user asked for. We need to move that data here somewhere else.
  //Note that the page at pgid currently might not have a SpaceNode, if the previous page was large enough.
  while(pageptr != NULL){ //No guarantees on page size, so we have to iterate through the linked list.
    if(pageptr->free == 0 && pageptr->pid == currid){
      //We found an allocated page for our pid, but is it the right one?
      if(pageptr->order <= pgid && pageptr->order + (getPageSize(pageptr) / PAGE_SIZE) >= pgid){
	//printf("Found needed page.\n");
	break;
      }
    }
    pageptr = pageptr->next;
  }
  
  
  if(pageptr == NULL){
    //The page was not found within memory. However, it may be in the swapfile. We'll have to check that.
    perror("Page not found in memory. Need to implement checking swapfile.\n");
    exit(1);
  }
  
  //So now, pageptr points to the page that the thread is requesting. pgid is the 'page index' where that page ought to be.
  //Now, let's move whatever is at the addresses of mem that pageptr needs to be at.
  
  SpaceNode * snptr = ((SpaceNode *)&mem[MEMORY_START]);
  SpaceNode * snprev;
  int pg = 0;
  
  while(pg <= pgid && snptr != NULL){
    pg += ((getPageSize(snptr)+sizeof(SpaceNode) - 1) / PAGE_SIZE) + 1;
    //That -1 is there because if the size happens to equal page_size - sizeof(spacenode), we only need one page still.
    snprev = snptr;
    snptr = snptr->next;
  }
  
  //Now, snprev points to where pageptr should be moved to, and snptr is that page's next.
  //Note that snprev could be multiple pages long, and pageptr needs to actually be moved to somewhere *inside* of it.
  
  //Find out how many pages need to be freed:
  int pagesneeded = ((getPageSize(pageptr) + sizeof(SpaceNode) - 1) / PAGE_SIZE) + 1;
  //The 'page index' of snprev:
  long snprev_pg_index = (long)  ( (long)snprev - (long)&mem[MEMORY_START] ) / PAGE_SIZE;
  pagesneeded -= (pgid - snprev_pg_index) + 1; //We'll have to move snprev at any rate, so the nubmer of extra pages we'll need decreases by the pages snprev has.
  //However, snprev could be free space, and pageptr wants to go int the middle of that instead of the beginning. I just pagesneeded appropriately to account for this.
  if(pagesneeded > 0 && snprev->next == NULL){
    //This could potentially happen if allocation gives a virtual memory pointer by the very end of memory, but the size of the page is too big.
    //I guess this another thing translation will need to account for. I haven't done that.
    printf("need more pages but we're at the end of mem...\n");
    exit(1);
  }
  
  //If snprev isn't free, call allocate on it so you can free up space.
  
  if(snprev->free == 0){
    SpaceNode * temp = (SpaceNode *)myallocate(getPageSize(snprev), __FILE__, __LINE__, VMREQ);
    //This malloc call returns a real address, not virtual.
    if(temp == NULL){
      perror("Couldn't find enough space to swapp.\n");
      exit(1);
    }
    memcpy(temp->start, snprev->start, snprev->size);
    temp--;
    temp->pid = snprev->pid;
    //Additional swap operations are necessary. Namely, appropriately marking snprev as free (possible extending a previous free spacenode)
    //As well as updating the pointers in the metadata of the new page.
    //I don't think a standard mydeallocate would work, since that expects a pointer to some data, not a spacenode.
  }
  //}
  
  //We moved over snprev, but we still don't have enough pages available for our desired page.
  
  while(pagesneeded > 0){ //start moving stuff stuff starting at snptr
    if(snptr == NULL){
      //Again, translation would have given a virtual address with less room than was needed.
      perror("Required page size is leaking off the edge of mem.\n");
      exit(1);
    }
    pagesneeded -= ((getPageSize(snptr) + sizeof(SpaceNode) - 1) / PAGE_SIZE) + 1;
    if(snptr->free == 1){
      snptr = snptr->next;
    }
    else {
      SpaceNode * temp = (SpaceNode *)myallocate(getPageSize(snptr), __FILE__, __LINE__, VMREQ);
      //It's possible this allocate call found a 'free' address either in snprev or another page
      //we had just freed in an earlier loop. I haven't implemented anything to account for this yet.
      if(temp == NULL){
	perror("Couldn't find enough space to swap.\n");
	exit(1);
      }
      memcpy(temp, snprev->start,snprev->size);
      temp--;
      temp->pid = snptr->pid;
      temp = snptr;
      snptr = snptr->next;
      //Again, more swap operations are necessary.
      
      if(snptr == pageptr){
	//We happened to move over a the page that we want to swap in. We need to update pageptr to the new location in this case.
	pageptr = temp;
      }
      
    }
  }
  
  //Now we need to get the address where we'll be plopping the node down. Recall that pageptr is where the data is currently residing. We want to move it to newloc.
  SpaceNode * newloc = (SpaceNode *) (((long)&mem[MEMORY_START]) + (PAGE_SIZE * pgid));
  
  if (pagesneeded < 0){ //This means that the last page we removed was larger than we needed space for. Now we need to add in free space pagenode.
    printf("Running freeloc stuff.\n");
    //I'm not exactly sure what a free spacenode should look like. I gathered it was something like this.
    SpaceNode * freeloc = (SpaceNode *) (((long)newloc) + sizeof(SpaceNode) + getPageSize(pageptr)); //This should fall right on a page.
    freeloc->start = (char *)(freeloc + 1);
    if(snptr == NULL){
      //freeloc becomes the last spacenode in mem.
      freeloc->size = (PHYSICAL_SIZE + &mem[MEMORY_START]) - ((long)freeloc->start);
    } else {
      freeloc->size = ((long)snptr) - ((long)freeloc->start);
    }
    freeloc->free = 1;
    freeloc->next = snptr;
    freeloc->prev = newloc;
    newloc->next = freeloc;
  }
  
  //Will we be putting down memory in the middle of some free space?
  if(newloc == snprev){
    //Great, where we want to put the page already lines up with the beginning of some free space.
    //If the free space was bigger than the pagesize, that should have been covered in the previous if statement.
  } else {
    //We need to do some pointer gynmnastics first.
    snprev->next = newloc;
    snprev->size = ((long)newloc) - ((long)snprev->start);
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
  
  //Need to have some code to free stuff up at pageptr. Also, we need to do things like update metadata pointers.
  
  //mprotect everything except the page we just swapped.
  
  /*mprotect(&mem[MEMORY_START],(int)(((char *)newloc) - &mem[MEMORY_START]), PROT_NONE);
  if(newloc->next != NULL){
    mprotect(newloc->next,(&mem[PHYSICAL_SIZE - 1]) - (int)newloc->next, PROT_READ | PROT_WRITE);
    }*/
  int tempsize = newloc->size; //Won't be able to dereference newloc->size otherwise
  mprotect(&mem[MEMORY_START], PHYSICAL_SIZE - MEMORY_START, PROT_NONE);
  mprotect(newloc, tempsize + sizeof(SpaceNode), PROT_READ | PROT_WRITE);
  inMemFun = 0;
  
  //Lastly, re-enable interrupts.
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}
