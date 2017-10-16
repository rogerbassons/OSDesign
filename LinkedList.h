#include <ucontext.h>

typedef struct sthread sthread;
struct sthread;
typedef sthread* my_pthread_t;

typedef struct node {
	my_pthread_t *thread;	
	struct node *next;
	struct node *prev;
} Node;

typedef struct {
	struct node *tail;
	struct node *head;
	unsigned int nitems;
} LinkedList;

void push(LinkedList *l, my_pthread_t *t);
void pushOrdered(int order, LinkedList *l, my_pthread_t *t);
void orderByOldest(LinkedList *l);
my_pthread_t *pop(LinkedList *l);

my_pthread_t *front(LinkedList *l);
int empty(LinkedList *l);
unsigned int nelements(LinkedList *l);
void print(LinkedList *l);

