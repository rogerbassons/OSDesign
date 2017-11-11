#ifndef MY_VM_H
#define MY_VM_H

#include <stdio.h>
#include <signal.h>
#define _GNU_SOURCE

#define THREADREQ 0
#define OSREQ -1
#define SWAPREQ -2


#define PHYSICAL_SIZE  8000000 //8MB
#define MEMORY_START   2000000 // first 2MB are OS-reserved
static char mem[PHYSICAL_SIZE]; // physical memory
struct sigaction saSeg;

/* 
Allocate memory block
Allocates a block of size bytes of memory, returning a pointer to the beginning
of the block.

The content of the newly allocated block of memory is not initialized, remaining
with indeterminate values.

If size is zero, returns a null pointer.
*/
void* myallocate (size_t size, char *file, int line, int request);

/* 
Deallocate memory block
A block of memory previously allocated by a call to myallocate is deallocated,
making it available again for further allocations. 

If ptr does not point to a block of memory allocated with the above functions,
it causes undefined behavior. 

If ptr is a null pointer, the function does nothing.

Notice that this function does not change the value of ptr itself, hence it
still points to the same (now invalid) location. 
*/ 
void mydeallocate(void* ptr, char *file, int line, int request);

void printMemory();

void memfun(int sig, siginfo_t *si, void *unused);


#ifdef THREADREQ
#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)
#endif

#endif
