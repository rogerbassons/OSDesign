#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>

#define PAGE 0
#define ELEMENT 1



static char mem[PHYSICAL_SIZE] = "";

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

void *myallocate (size_t size, char *file, int line, int request)
{

	if (mem[0] == 0) { //first run
		mem[0] = 1;


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
			break;
	        case OSREQ:
			// allocate memory to OS data
			ptr = getFreeOSElement(size);
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



