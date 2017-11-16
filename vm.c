#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>


#define PAGE 0
#define ELEMENT 1



FILE *f = NULL;

typedef struct spaceNode {
	unsigned pid;
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

SpaceNode *getNextSpace(SpaceNode *old)
{
	if (old == NULL)
		return NULL;
	else
		return old->next;
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
	new.split = 0;
	new.next = new.prev = NULL;
	new.size = n->size - sizeof(SpaceNode);
	new.unusedSize = 0;
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
		if (restSize > sizeof(SpaceNode)) {

			SpaceNode new;
			new.free = 1;
			new.split = 0;
			new.prev = n;
			new.next = n->next;
			new.pid = 0;
			new.size = restSize;
			new.unusedSize = 0;

			void *newPos = n->start + size;
			new.start = newPos + sizeof(SpaceNode);
		
			memcpy(newPos, &new, sizeof(SpaceNode));
		
			n->next = (SpaceNode *) newPos;
		}
		n->size = size;
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
	
	size_t s = 0;
	SpaceNode *old = NULL;
	while (s < size) {
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



char *getSwap()
{
	// Create/Open Swap file
	FILE *f = fopen("swap", "r+b");
	if (f == NULL) {
		perror("Error creating swap file");
		return NULL;
	}

	static char swap[SWAP_SIZE];

	if (fread(swap, sizeof(swap), 1, f) != 1) {
		fprintf(stderr, "Error: Failed to read swap file\n");
		return NULL;
	}
	restorePagePointers((SpaceNode *) &swap[0], SWAP_SIZE);
	fclose(f);
	return swap;
	
}

int writeSwap(char *swap)
{
	// Create/Open Swap file
	FILE *f = fopen("swap", "w+b");
	if (f == NULL) {
		perror("Error creating/opening swap file");
		return 1;
	}
		

	if (fwrite(swap, SWAP_SIZE, 1, f) != 1) {// write swap array
		printf("Error writing to swap file");
		return 1;
	}

	return fclose(f);
}


int movePageToSwap()
{

	SpaceNode *lru = getFirstPage();//TODO findLRUPage(getFirstPage());

	// READ SWAP FILE	
	char *swap = getSwap();


	// Find free swap page
	SpaceNode *freeSwapPage = findFreeSpace((SpaceNode *)&swap[0], lru->size);
	if (freeSwapPage == NULL) {
		fprintf(stderr, "Error: No free pages\n");
		return 1;
	}

	lru->next = freeSwapPage->next;
	lru->prev = freeSwapPage->prev;

	memcpy(freeSwapPage, &lru, sizeof(SpaceNode) + lru->size);
	writeSwap(swap);

	// free memory LRU page
	removeSpace(lru, PAGE);

	return 0;
}

// p1 and p2 are the same size
// swaps pages p1 and p2
int swapPages(SpaceNode *p1, SpaceNode *p2)
{
	if (p1 == p2 || p1->size != p2->size) {
		return 1;
	}
	size_t pageSize = p1->size + sizeof(SpaceNode);
	char copy[pageSize];

	void *start = p1->start;
	SpaceNode *next = p1->next;
	SpaceNode *prev = p1->prev;


	memcpy((char *) copy, (char *) p1, pageSize);

	memmove((void *) p1, (void *) p2, pageSize);
	p1->next = next;
	p1->prev = prev;
	p1->start = start;
	restoreElementsPointers(p1->start, p1->size);

	start = p2->start;
	next = p2->next;
	prev = p2->prev;

	memmove((void *) p2, (void *) copy, p2->size + sizeof(SpaceNode));
	p2->next = next;
	p2->prev = prev;
	p2->start = start;
	restoreElementsPointers(p2->start, p2->size);


	return 0;
}


void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreePage();
	
	
	if (n == NULL) {
		printf("No free pages, moving pages to swap...\n"); //TODO DEVEL
		if (movePageToSwap()) {
			return NULL;
		} else {
			return getFreePage(size, pid);
		}
		return NULL;

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

void *getFreeOSElement(size_t size)
{
	SpaceNode *n = findFreeSpace((SpaceNode *)mem, size);

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


// page1 is page1 + page2
void makeContiguous(SpaceNode *page1, SpaceNode *page2)
{
	if (page1 != page2) {
		SpaceNode *contiguousPage = page1->next;

		swapPages(contiguousPage, page2);
			

		size_t size = page2->size;
	
		void *dataStart = ((void *)page1) + sizeof(SpaceNode) + page1->size;
		page1->size += page2->size;
		page1->unusedSize = sizeof(SpaceNode);
		page1->next = page2->next;

	
		memmove(dataStart, page2->start, size);
		restoreElementsPointers(page1->start, page1->size);
		joinFreeMemory(page1->start);
	}
	      
}

// splits p1 into system page sized pages
void splitPages()
{

	SpaceNode *p = getFirstPage();
	size_t pageSize = sysconf( _SC_PAGE_SIZE);
	size_t size = p->size + sizeof(SpaceNode) + p->unusedSize;


	if (size > pageSize) {
		void *pages = getFirstPage();
		char copy[size];
		memcpy(copy, p, size); // copy multiple pages "page" to copy

		SpaceNode new;
		new.free = 0;
		new.order = 0;
		new.split = 1;
		new.pid = p->pid;
		new.size = pageSize - sizeof(SpaceNode);
		new.unusedSize = 0;
		new.next = NULL;
		
	
		void *start = pages;
		void *copyStart = copy;
		SpaceNode *prev = NULL;
		int i = 0;
		while (size >= pageSize) {

			if (prev != NULL)
				prev->next = start;
			new.prev = prev;
		
			new.start =  start + sizeof(SpaceNode);

			new.order += 1;
			memmove(start, &new, sizeof(SpaceNode));
			prev = (SpaceNode *) start;
			start += sizeof(SpaceNode);

		
			copyStart += sizeof(SpaceNode);
			memmove(start, copyStart, new.size);
			start += new.size;
			copyStart +=  new.size;

			i++;
			size -= pageSize;
		}
		if (prev != NULL)
			prev->next = start;
	}
}

void joinPages(int pid)
{
	//TODO
}

int reserveAnotherPage(SpaceNode *page)
{
	unsigned pid = page->pid;
	
	SpaceNode *newPage = getFreePage(sysconf( _SC_PAGE_SIZE), page->pid);
	if (newPage == NULL) {
		printf("No free pages while reserving another page\n");
		return 1;
	}

	page = findPage(pid, NULL, 2);
	makeContiguous(newPage, page);

	return 0;
}

void *getFreeElement(size_t size)
{
	unsigned pid = 1;
	if (running != NULL)
		pid = (*running)->id;

	SpaceNode * p = findProcessPage(pid, NULL);
	if (p == NULL) {
		perror("Cannot find process page");
		return NULL;
	}

	if (VIRTUAL_MEMORY) {
		swapPages(getFirstPage(), p);
		p = getFirstPage();
	}

	SpaceNode *n = findFreeSpace((SpaceNode *)(p->start), size);

	if (n == NULL) {
		if (VIRTUAL_MEMORY) {
			printf("No free space inside the thread's page, reserving another one\n"); //devel TODO
			if (reserveAnotherPage(p)) {
				perror("Error reserving another page: no free space");
				return NULL;
			}
			return getFreeElement(size);
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

void printSwap()
{
	printData(getSwap(), 1);
}

void printMemory()
{
	printData(getFirstPage(), 0);

}
 
void initializeFreePages()
{
	size_t pageSize = sysconf(_SC_PAGE_SIZE); 
	SpaceNode i;
	i.free = 1;
	i.unusedSize = 0;
	i.split = 0;
	i.next = i.prev = NULL;
	i.size = pageSize - sizeof(SpaceNode);
	i.pid = 0;

	SpaceNode *prev = NULL;
	size_t freeSpace = PHYSICAL_SIZE - MEMORY_START;
	void *start = mem + MEMORY_START;

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
}

int memoryAllow()
{
	size_t pageSize = sysconf(_SC_PAGE_SIZE);
	mprotect(mem + MEMORY_START, pageSize, PROT_READ | PROT_WRITE);
	SpaceNode *first = getFirstPage();
	if (first->size > pageSize - sizeof(SpaceNode)) {
		mprotect(mem + MEMORY_START, first->size, PROT_READ | PROT_WRITE);
	}
}


static void handler(int sig, siginfo_t *si, void *unused)
{
	//printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);

	long addr = (long) si->si_addr;

	long first = (long) mem + MEMORY_START;
	long last = (long) mem + PHYSICAL_SIZE;
	
	if (addr >= first && addr < last) {

		memoryAllow();
		splitPages();
		// move all running thread's pages to the beginning and make them contiguous

		unsigned pid = (*running)->id;

		SpaceNode *p = findProcessPage(pid, NULL);
		if (p != NULL) {
			swapPages(getFirstPage(), p);
			
			SpaceNode *current = getFirstPage();
			p = findProcessPage(pid, current->next);
			while (p != NULL) {
				makeContiguous(current, p);
				p = findProcessPage(pid, current->next);
			}
		}

		
	} else {
		sigaction(SIGSEGV, &oldSIGSEGV, NULL);
	}
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
		
	// System reserved space
	SpaceNode new;
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = MEMORY_START - sizeof(SpaceNode) - 1;
	new.start = mem + sizeof(SpaceNode);
	new.pid = 0;

	memcpy(mem, &new, sizeof(SpaceNode));



	// Free space for pages
	initializeFreePages();


	// set SIGSEGV handler
	if (VIRTUAL_MEMORY)
		setSIGSEGV();

	// Create Swap file
	static char swap[SWAP_SIZE] = "";
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = SWAP_SIZE - sizeof(SpaceNode);
	new.start = swap + sizeof(SpaceNode);
	new.pid = 0;
	memcpy(swap, &new, sizeof(SpaceNode));
	writeSwap(swap);
		
}

void *myallocate (size_t size, char *file, int line, int request)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa->sa_mask, &oldmask);

	if (mem == NULL) { //first run
		init();
	
	}

	void *ptr = NULL;
	switch (request) {
	case THREADREQ:
		// allocate inside a thread page
		ptr = getFreeElement(size);
		break;
	case OSREQ:
		// allocate memory to OS data
		ptr = getFreeOSElement(size);
		break;
	default:
		// reserve a page to a thread with id request
		ptr = getFreePage(size, request);

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



