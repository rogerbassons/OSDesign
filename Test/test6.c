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
	unsigned order;
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

void loadRunningProcessPages();

int main()
{
	mainThread = (sthread *) myallocate(sizeof(sthread), "my_pthread.c", 0, OSREQ);
	mainThread->id = 1;
	mainThread->priority = 0;
	mainThread->born = (unsigned long)time(NULL);
	mainThread->function = NULL;
	mainThread->arg = NULL;
	mainThread->finished = 0;
	mainThread->pages = myallocate(sysconf( _SC_PAGE_SIZE), "my_pthread.c", 0, 1);

	running = (my_pthread_t *) myallocate(sizeof(my_pthread_t), "my_pthread.c", 0, OSREQ);
	*running = mainThread;

	void *x = myallocate(11900, "my_pthread.c", 0, 0);

	printMemory();
	splitPages(); // scheduled out
	printMemory();

	loadRunningProcessPages();

	printMemory();
	return 0;
}
