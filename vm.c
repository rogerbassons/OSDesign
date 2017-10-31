#include <stdlib.h>
#include "vm.h"
#include "my_pthread_t.h"
#include <string.h>

typedef struct spaceNode {
	unsigned free;
	char *start;
	unsigned size;
	struct spaceNode *next;
	struct spaceNode *prev;
} SpaceNode;


SpaceNode *getNextSpace(SpaceNode *old)
{
	SpaceNode *n = NULL;
	if (old == NULL)
		n = (SpaceNode *) &mem[0];
	else
		n = old->next;

	return n;
}

SpaceNode *findFreeSpace(size_t size)
{
	SpaceNode *n = getNextSpace(NULL);

	while (n != NULL && !(n->free) && n->size < size)
		n = getNextSpace(n);

	if (n == NULL)
		return NULL;
	else if (n->free && n->size > size)
		return n;

	
}

int createSpace(SpaceNode *n, size_t size)
{
	SpaceNode *b = n->next;
	SpaceNode new;
	new.free = 1;
	new.prev = (SpaceNode *)&n;
	new.next = b;

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
		return 0;
	}	
}


void *getFreePage(size_t size, unsigned pid)
{
	SpaceNode *n = findFreeSpace(size);
	
	if (n == NULL) {
		return NULL;
	} else {
		
		if (createSpace(n, size)) {
			perror("Error creating space");
			return NULL;
		}
			
		//setProcessPage(pid);
		return (void *)n->start;
	}
	return (void *)n->start;
}

void *getFreeThread(size_t size)
{
	//unsigned pid = (*running)->id;
	unsigned pid = 1;

	//getProcessPage(pid);
	//TODO
	return NULL;
}

void printMemory(size_t size)
{
	SpaceNode *n = getNextSpace(NULL);

	int i = 1;
	while (n != NULL) {
		if (n->free)
			printf("----- Free Space -----\n", i);
		else {
			printf("----- Page %i -----\n", i);
			i++;
		}
		printf("Size: %i\n", n->size);
		if (!n->free) {
			printf("Contents:\n");
			int j = 0;
			printf("| ");
			while (j < n->size) {
				char *ptr = (n->start + j);
				if (*ptr != 0)
					printf("%i | ", *ptr);
				else
					printf("NULL | ");
				
				j++;
			}
			printf("\n");
		}
		n = getNextSpace(n);	


	}

}

void *myallocate (size_t size, char *file, char *line, int request)
{

	if (first == NULL) {
		int a = 1;
		first = &a;

		SpaceNode new;

		new.free = 1;
		new.next = new.prev = NULL;
		new.size = PHYSICAL_SIZE - sizeof(SpaceNode);
		new.start = &mem[0] + sizeof(SpaceNode);

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

int removePage(void *ptr)
{
	SpaceNode *n = ptr - sizeof(SpaceNode);

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

void mydeallocate(void* ptr, char *file, char *line, int request)
{
	switch (request) {
		case THREADREQ:
			// deallocate

			break;
		default:
			// deallocate a page
			removePage(ptr);
			printf("\nDeallocating page...\n\n");
			printMemory(0);

			
	}
}



