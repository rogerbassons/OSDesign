#include "LinkedList.h"
#include <stdlib.h>
#include <stdio.h>

int getPriority(my_pthread_t *t);
unsigned int getTimeCreated(my_pthread_t *t);

Node *newNode(LinkedList *l, my_pthread_t *t) {
	Node *new = (Node*)malloc(sizeof(Node));
	new->thread = (my_pthread_t *) malloc(sizeof(my_pthread_t));
	*(new->thread) = *t;
	new->next = NULL;
	return new;
}


void push(LinkedList *l, my_pthread_t *t) {
	Node* n = newNode(l,t);
	//printf("push: %p\n\n", *t);
	if (l->tail == NULL) {
		l->nitems = 1;
		l->head = l->tail = n;
	} else {
		l->tail->next = n;
		l->tail = n;
		l->nitems++;
	}
	
}


void pushOrdered(int order, LinkedList *l, my_pthread_t *t) 
{
	Node* n = newNode(l,t);
	//printf("push: %p\n\n", *t);
	if (l->tail == NULL) {
		// empty list
		l->nitems = 1;
		l->head = l->tail = n;
	} else {
		Node *tmp = l->head;
		Node *last = tmp;

		if (order == 0) {
			while (tmp != NULL && getPriority(tmp->thread) <= getPriority(n->thread)) {
				last = tmp;
				tmp = tmp->next;
			}
		} else {
			while (tmp != NULL && getTimeCreated(tmp->thread) <= getTimeCreated(n->thread)) {
				last = tmp;
				tmp = tmp->next;
			}
		}
		
		n->next = last->next;

		last->next = n;

		if (n->next == NULL) {
			l->tail = n;
		}

		if (n->next == l->head) {
			l->head = n;
		}
		l->nitems++;

	}
	
}

void orderByOldest(LinkedList *l)
{
	LinkedList *new = (LinkedList *) malloc(sizeof(LinkedList));
	while (!empty(l)) {
		my_pthread_t *t = pop(l);
		pushOrdered(1, new, t);
	}
	*l = *new;
}

my_pthread_t *pop(LinkedList *l) {
	Node *n = l->head;
	l->head = l->head->next;
	
	if (l->head == NULL) {
		l->tail = NULL;
	}
	
	//my_pthread_t t = n->threadId;
	//free(n);
	l->nitems--;
	//printf("pop: %p\n\n", *(n->thread)); 
	return n->thread;
}


int empty(LinkedList *l) {
	return l->head == NULL;
}

my_pthread_t *front(LinkedList *l) {
	return l->head->thread;
}

unsigned int nelements(LinkedList *l) {
	return l->nitems;
}

void print(LinkedList *l) {
	Node *n = l->head;
	printf("LIST:\n\n");
	while (n != NULL) {
		printf("%p | ", *(n->thread));
		n = n->next;
	}
	printf("ENDLIST\n\n");
}



