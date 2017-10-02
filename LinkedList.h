#include <ucontext.h>

typedef struct node {
	ucontext_t context;	
	struct node *next;
	struct node *prev;
} Node;

typedef struct {
	struct node *tail;
	struct node *head;
	unsigned int nitems;
} LinkedList;

void push(LinkedList *l, ucontext_t c);

ucontext_t pop(LinkedList *l);

ucontext_t front(LinkedList *l);
int empty(LinkedList *l);
unsigned int nelements(LinkedList *l);
void print(LinkedList *l);

