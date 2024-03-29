#ifndef MY_VM_H
#define MY_VM_H

#include <stdio.h>
#include <signal.h>

#define _GNU_SOURCE

#define THREADREQ 0
#define OSREQ -1
#define SWAPREQ -2
#define SHALLOC -3

#define VIRTUAL_MEMORY 1
#define PHYSICAL_SIZE  8000000 //8MB
#define SWAP_SIZE 4000000 // 16MB
#define SWAP_STRATEGY 1 // 0 -> naive, 1 -> 2n chance
#define USE_SWAP_F 1
#define USE_FIND_IN_SWAP 0
#define OSRESERVED_START 1000000
#define SWAP_MEMORY_START 1000000
#define MEMORY_START   2097152 // first ~2MB are OS-reserved
void *sharedMemoryStart;
char *mem; // physical memory
struct sigaction oldSIGSEGV;
size_t pageSize;

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

void* shalloc(size_t size);

void updateReference();

int memoryProtect(unsigned pid);

void printOSMemory();
void printMemory();
void printSwap();
void printSwapF(int k);


#ifdef THREADREQ
#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)
#endif

#endif
