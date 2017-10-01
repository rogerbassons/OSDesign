#include "DynamicContextArray.h"

void createArray(DynamicContextArray *a, size_t size) {
  a->array = (ucontext_t **)malloc(size * sizeof(ucontext_t*));
  a->last = 0;
  a->size = size;
}

void insertArray(DynamicContextArray *a, ucontext_t *c) {
  if (a->size == a->last) {
    a->size *= 2;
    a->array = (ucontext_t **)realloc(a->array, a->size * sizeof(ucontext_t*));
  }
  a->array[a->last++] = c;
}

void freeArray(DynamicContextArray *a) {
  free(a->array);
  a->array = NULL;
  a->last = a->size = 0;
}

DynamicContextArray run, wait;
