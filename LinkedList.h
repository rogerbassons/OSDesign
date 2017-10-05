#include <ucontext.h>

typedef struct node {
	my_thread *thread;	
	struct node *next;
	struct node *prev;
} Node;

typedef struct {
	struct node *tail;
	struct node *head;
	unsigned int nitems;
} LinkedList;

void push(LinkedList *l, my_thread *t);

thread *pop(LinkedList *l);

thread *front(LinkedList *l);
int empty(LinkedList *l);
unsigned int nelements(LinkedList *l);
void print(LinkedList *l);

