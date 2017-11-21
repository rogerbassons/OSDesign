#include "../my_pthread_t.h"
#include "../vm.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

typedef struct spaceNode {
	unsigned pid;
	unsigned free;
	int order;
	unsigned split;
	void *start;
	unsigned size;
	unsigned unusedSize;
	struct spaceNode *next;
	struct spaceNode *prev;
} SpaceNode;

SpaceNode *getFirstPage();
void makeContiguous(SpaceNode *page1, SpaceNode *page2);
void swapPages(SpaceNode *, SpaceNode*);
SpaceNode *findProcessPage(unsigned pid, SpaceNode *start);
SpaceNode *findSplitPage(unsigned pid, int number);
int splitPages();

void loadProcessPages();

int main()
{
	mainThread = (sthread *) myallocate(sizeof(sthread), "my_pthread.c", 0, OSREQ);
	mainThread->id = 1;
	mainThread->priority = 0;
	mainThread->born = (unsigned long)time(NULL);
	mainThread->function = NULL;
	mainThread->arg = NULL;
	mainThread->finished = 0;
	myallocate(0, "my_pthread.c", 0, 1);

	running = (my_pthread_t *) myallocate(sizeof(my_pthread_t), "my_pthread.c", 0, OSREQ);

	*running = mainThread;
	char *a = myallocate(100, "my_pthread.c", 0, THREADREQ);

	printMemory();
	

	pthread_t t = (sthread *) myallocate(sizeof(sthread), "my_pthread.c", 0, OSREQ);
	t->id = 2;
	t->priority = 0;
	t->born = (unsigned long)time(NULL);
	t->function = NULL;
	t->arg = NULL;
	t->finished = 0;
	myallocate(0, "my_pthread.c", 0, 2);

	*running = t;
	char *b = myallocate(1200, "my_pthread.c", 0, THREADREQ); // this will need multiple pages

	printMemory();
	loadProcessPages(mainThread->id);
	printMemory();


	loadProcessPages(t->id);
	printMemory();
	myallocate(4000, "my_pthread.c", 0, THREADREQ); // more pages
	printMemory();
	
	return 0;
}
