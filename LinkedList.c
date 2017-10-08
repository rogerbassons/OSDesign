#include "LinkedList.h"
#include <stdlib.h>
#include <stdio.h>

Node *newNode(LinkedList *l, my_pthread_t *t) {
	Node *new = (Node*)malloc(sizeof(Node));
	new->thread = t;
	new->next = NULL;
	return new;
}


void push(LinkedList *l, my_pthread_t *t) {
	Node* n = newNode(l,t);
	if (l->tail == NULL) {
		l->nitems = 0;
		l->head = l->tail = n;
	} else {
		l->tail->next = n;
		l->tail = n;
		l->nitems++;
	}
	
}

my_pthread_t *pop(LinkedList *l) {
	Node *n = l->head;
	l->head = l->head->next;
	
	if (l->head == NULL) {
		l->tail = NULL;
	}
	
	my_pthread_t *t = n->thread;
	free(n);
	l->nitems--;
	return t;
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
	while (n != NULL) {
		printf("%p\n", n->thread);
		n = n->next;
	}
}
