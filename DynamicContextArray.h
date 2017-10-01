#include <ucontext.h>
#include <stdlib.h>

typedef struct {
  ucontext_t **array;
  size_t last;
  size_t size;
} DynamicContextArray;

void createArray(DynamicContextArray *a, size_t size);

void insertArray(DynamicContextArray *a, ucontext_t *c);

void freeArray(DynamicContextArray *a);



