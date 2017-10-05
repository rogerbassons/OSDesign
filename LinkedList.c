#include "LinkedList.h"
#include <stdlib.h>
#include <stdio.h>

Node *newNode(LinkedList *l, my_thread *t) {
	Node *new = (Node*)malloc(sizeof(Node));
	new->thread = t;
	new->next = NULL;
	return new;
}


void push(LinkedList *l, my_thread *t) {
	Node* n = newNode(l,t);
	printf("PUSH: %p\n", &(t.uc_stack));
	if (l->tail == NULL) {
		l->nitems = 0;
		l->head = l->tail = n;
	} else {
		l->tail->next = n;
		l->tail = n;
	}
	l->nitems++;
}

thread *pop(LinkedList *l) {
	Node *n = l->head;
	l->head = l->head->next;
	
	if (l->head == NULL) {
		l->tail = NULL;
	}
	
	my_thread *t = n->thread;
	free(n);
	printf("POP: %p\n", &(t->context.uc_stack));
	l->nitems--;
	return t;
}


int empty(LinkedList *l) {
	return l->head == NULL;
}

my_thread *front(LinkedList *l) {
	return l->head->context;
}

unsigned int nelements(LinkedList *l) {
	return l->nitems;
}

void print(LinkedList *l) {
	Node *n = l->head;
	while (n != NULL) {
		printf("ELEMENT: %p\n", &(n->thread->context.uc_stack));
		n = n->next;
	}
}
