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
	char *start;
	unsigned size;
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
	new.next = new.prev = NULL;
	new.size = n->size - sizeof(SpaceNode);
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
			new.prev = n;
			new.next = n->next;
			new.pid = 0;
			new.size = restSize;

			char *newPos = n->start + size;
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

void restorePointers(SpaceNode *n, size_t size)
{

	size_t s = 0;
	SpaceNode *old = NULL;
	while (s < size) {
		size_t offset = sizeof(SpaceNode) + n->size;

		SpaceNode *next = n + offset;
		n->next = next;
		n->prev = old;

		n = next;
		s += offset;
	}
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
	restorePointers((SpaceNode *) &swap[0], SWAP_SIZE);
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

// swaps pages p1 and p2
int swapPages(SpaceNode *p1, SpaceNode *p2)
{
	if (p1 == p2) {
		return 1;
	}
	
	size_t pageSize = p1->size + sizeof(SpaceNode);
	char page1[pageSize];
	memcpy(page1, p1, pageSize);
	SpaceNode *backup = (SpaceNode *) &page1;

	memcpy(p1, p2, pageSize);
	p1->next = backup->next;
	p1->prev = backup->prev;


	backup->next = p2->next;
	backup->prev = p2->prev;
	memcpy(p2, page1, pageSize);

	return 0;
}


void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreePage();
	
	
	if (n == NULL) {
				
		if (movePageToSwap()) {
			return NULL;
		} else {
			return getFreePage(size, pid);
		}

	} else {
		
		if (createSpace(n, size, PAGE)) {
			fprintf(stderr, "Error creating space for process %i\n", pid);
			return NULL;
		}
			
		setProcessPage(n, pid);

		swapPages(getFirstPage(), n);

		
		return (void *)getFirstPage();
	}
}

SpaceNode *findProcessPage(unsigned pid, SpaceNode *start)
{

	SpaceNode *n = getFirstPage();
	if (start != NULL)
		n = start;
	
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


// page1 is page1 + page2
void makeContiguous(SpaceNode *page1, SpaceNode *page2)
{
	SpaceNode *contiguousPage = page1->next;

	swapPages(contiguousPage, page2);

	void *dataStart = page1 + sizeof(SpaceNode) + page1->size;
	page1->size += page2->size;
	page1->next = page2->next;

	
	memcpy(dataStart, page2->start, page2->size);
}

// splits p1 into system page sized pages
void splitPages(SpaceNode *p)
{
	size_t pageSize = sysconf( _SC_PAGE_SIZE);
	size_t dataSize = pageSize - sizeof(SpaceNode);

	size_t size = p->size + sizeof(SpaceNode);
	char copy[size];
	memcpy(copy, p, size); // copy whole "page" to copy


	int i = 0;
	while (size > pageSize) {
		SpaceNode new;

		void *start = p + pageSize;

		new.free = 0;
		new.next = p->next;
		new.prev = p;
		new.size = pageSize;
		new.start =  start + sizeof(SpaceNode);
		new.pid = p->pid;

		memcpy(start, &new, sizeof(SpaceNode));
		memcpy(start + sizeof(SpaceNode), &copy + pageSize + dataSize*i, dataSize);
		
		i++;
		size -= pageSize;
	}
}

int reserveAnotherPage(SpaceNode *page)
{
	size_t pageSize = page->size + sizeof(SpaceNode);
		
	SpaceNode *newPage = getFreePage(sysconf( _SC_PAGE_SIZE), page->pid);
	if (free == NULL)
		return 1;


	SpaceNode *first = getFirstPage();
	
	if (page != first)
		swapPages(first, page);

	page = first;

	
	makeContiguous(page, newPage);

	return 0;
}

void *getFreeElement(size_t size)
{
	unsigned pid = (*running)->id;

	SpaceNode * p = findProcessPage(pid, NULL);
	if (p == NULL) {
		perror("Cannot find process page");
		return NULL;
	}


	swapPages(getFirstPage(), p);
	p = getFirstPage();
	
	SpaceNode *n = findFreeSpace((SpaceNode *)(p->start), size);
	
	if (n == NULL) {
		if (reserveAnotherPage(p)) {
			perror("Error reserving another page: no free space");
			return NULL;
		}
			
		return getFreeElement(size);
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

int printData(int type)
{
	SpaceNode *n;
	if (type == 0) {
		n = getFirstPage();
		printf("Memory: \n----------------------------------\n");
	} else {
		// READ SWAP FILE	
		char *swap = getSwap();
		n = (SpaceNode *)&swap[0];
		printf("Swap: \n----------------------------------\n");
	}
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
	return 0;
}

void printSwap()
{
	printData(1);
}

void printMemory()
{
	printData(0);

}
 
void initializeFreePages()
{
	size_t pageSize = sysconf(_SC_PAGE_SIZE); 
	SpaceNode i;
	i.free = 1;
	i.next = i.prev = NULL;
	i.size = pageSize - sizeof(SpaceNode);
	i.pid = 0;

	SpaceNode *prev = NULL;
	size_t freeSpace = PHYSICAL_SIZE - MEMORY_START - 1;
	void *start = &mem[MEMORY_START];

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
	SpaceNode *n = (SpaceNode *) page;
	return mprotect(page, n->size, PROT_NONE);  //disallow all accesses of address buffer over length pagesize
}

int memoryAllow(void *page)
{
	SpaceNode *n = (SpaceNode *) page;
	return mprotect(page, n->size, PROT_READ | PROT_WRITE); //allow read and write to address buffer over length pagesize
}

static void handler(int sig, siginfo_t *si, void *unused)
{
	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);

	long addr = (long) si->si_addr;
	long last = (long)(&mem[0] + PHYSICAL_SIZE);
	
	if (addr >= MEMORY_START && addr <= last) {

		splitPages(getFirstPage()); // restore last thread pages (make them non-contiguous)


		// move all running thread's pages to the beginning and make them contiguous
		unsigned pid = (*running)->id;
		SpaceNode *p = findProcessPage(pid, NULL);
		swapPages(getFirstPage(), p);

		SpaceNode *current = getFirstPage();
		p = findProcessPage(pid, current->next);
		while (p != NULL) {
			makeContiguous(current, p);
			p = findProcessPage(pid, current->next);
		}

		memoryAllow(current);
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
	void *p = &mem;
	posix_memalign(p, sysconf(_SC_PAGE_SIZE), PHYSICAL_SIZE);
		
	// System reserved space
	SpaceNode new;
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = MEMORY_START - sizeof(SpaceNode) - 1;
	new.start = &mem[1] + sizeof(SpaceNode);
	new.pid = 0;

	memcpy(&mem[1], &new, sizeof(SpaceNode));



	// Free space for pages
	initializeFreePages();


	// set SIGSEGV handler
	setSIGSEGV();

	// Create Swap file
	static char swap[SWAP_SIZE] = "";
	new.free = 1;
	new.next = new.prev = NULL;
	new.size = SWAP_SIZE - sizeof(SpaceNode);
	new.start = &swap[0] + sizeof(SpaceNode);
	new.pid = 0;
	memcpy(&swap[0], &new, sizeof(SpaceNode));
	writeSwap(swap);
		
	
}

void *myallocate (size_t size, char *file, int line, int request)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

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
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

	
	switch (request) {
	case THREADREQ:
		// deallocate
		removeSpace(ptr, ELEMENT);
		break;
	default:
		// deallocate a page
		removeSpace(ptr, PAGE);

	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}



