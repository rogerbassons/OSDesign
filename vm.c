#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>

#define PAGE 0
#define SEG 1

typedef struct spaceNode {
	unsigned pid;
	unsigned free:1;
	char *start;
	unsigned size;
	struct spaceNode *next;
	struct spaceNode *prev;
} SpaceNode;


SpaceNode *getFirstPage()
{
	return (SpaceNode *) &mem[0];
}

SpaceNode *getNextSpace(SpaceNode *old)
{
	if (old == NULL)
		return NULL;
	else
		return (SpaceNode *) old->next;
}

SpaceNode *findFreeSpace(SpaceNode *start, size_t size)
{
	SpaceNode *n;

	if (start == NULL) {
		n = getFirstPage();
	} else {
		n = start;
	}

	while (n != NULL && !(n->free) && n->size < size)
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
	SpaceNode *b = n->next;
	SpaceNode new;
	new.free = 1;
	new.prev = (SpaceNode *)&n;
	new.next = b;
	new.pid = 0;

	if (n->size < size)
		return 1;
	else {
		int restSize = n->size - size;
		if (restSize > sizeof(SpaceNode)) {
			n->size = size;
			new.size = restSize;

			void *pos = n->start + size;
			new.start = pos + sizeof(SpaceNode);

			n->next = (SpaceNode *) pos;
			memcpy(pos, &new, sizeof(SpaceNode));

		} 
		    

		n->free = 0;

		if (type == PAGE)
			initializeSpace(n); // create a free node inside the page
			
		return 0;
	}	
}


void setProcessPage(SpaceNode *n, int pid)
{
	n->pid = pid;
}

void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreeSpace(NULL, size);
	
	if (n == NULL) {
		return NULL;
	} else {
		
		if (createSpace(n, size, PAGE)) {
			perror("Error creating space");
			return NULL;
		}
			
		setProcessPage(n, pid);
		return (void *)n;
	}
}

SpaceNode *findProcessPage(unsigned pid)
{
	SpaceNode *n = getFirstPage();
	
	while (n != NULL && n->pid != pid)
		n = getNextSpace(n);

	if (n->pid == pid)
		return n;
	else
		return NULL;
}

void *getFreeThread(size_t size)
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
		
		if (createSpace(n, size, SEG)) {
			perror("Error creating space");
			return NULL;
		}
			
		return (void *)n->start;
	}
}

void printMemory(size_t size)
{
	SpaceNode *n = getFirstPage();

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
			printf("Contents:\n");

			SpaceNode *s = (SpaceNode *) n->start;
			int j = 1;
			while (s != NULL) {
				if (n->free)
					printf("----- Free Space -----\n");
				else {
					printf("----- Segment %i -----\n", j);
					j++;
				}
				printf("Size: %i\n", n->size);
				s = getNextSpace(s);
			}
		     
			
			printf("\n");
		}
		n = getNextSpace(n);	


	}

}

void *myallocate (size_t size, char *file, int line, int request)
{

	if (first == NULL) {
		int a = 1;
		first = &a;

		SpaceNode new;

		new.free = 1;
		new.next = new.prev = NULL;
		new.size = PHYSICAL_SIZE - sizeof(SpaceNode);
		new.start = &mem[0] + sizeof(SpaceNode);
		new.pid = 0;

		memcpy(&mem[0], &new, sizeof(SpaceNode));
	}

	void *ptr = NULL;
	switch (request) {
		case THREADREQ:
			// allocate
			ptr = getFreeThread(size);
			break;
		default:
			// reserve a page
			ptr = getFreePage(size, request);
			printf("\nAllocating page...\n\n");
			printMemory(size);

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
			removeSpace(ptr, SEG);
			break;
		default:
			// deallocate a page
			removeSpace(ptr, PAGE);
			printf("\nDeallocating page...\n\n");
			printMemory(0);

			
	}
}



