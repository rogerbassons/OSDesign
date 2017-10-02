#include "LinkedList.h"
#include <stdlib.h>
#include <stdio.h>

Node *newNode(LinkedList *l, ucontext_t c) {
	Node *new = (Node*)malloc(sizeof(Node));
	new->context = c;
	new->next = NULL;
	return new;
}


void push(LinkedList *l, ucontext_t c) {
	Node* n = newNode(l,c);
	printf("PUSH: %p\n", &(c.uc_stack));
	if (l->tail == NULL) {
		l->nitems = 0;
		l->head = l->tail = n;
	} else {
		l->tail->next = n;
		l->tail = n;
	}
	l->nitems++;
}

ucontext_t pop(LinkedList *l) {
	Node *n = l->head;
	l->head = l->head->next;
	
	if (l->head == NULL) {
		l->tail = NULL;
	}
	
	ucontext_t context = n->context;
	free(n);
	printf("POP: %p\n", &(context.uc_stack));
	l->nitems--;
	return context;
}


int empty(LinkedList *l) {
	return l->head == NULL;
}

ucontext_t front(LinkedList *l) {
	return l->head->context;
}

unsigned int nelements(LinkedList *l) {
	return l->nitems;
}

void print(LinkedList *l) {
	Node *n = l->head;
	while (n != NULL) {
		printf("ELEMENT: %p\n", &(n->context.uc_stack));
		n = n->next;
	}
}
