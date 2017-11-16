#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* To use real pthread Library in Benchmark, you have to comment the USE_MY_PTHREAD macro */
#define USE_MY_PTHREAD 1

#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <sys/time.h>
#include "LinkedList.h"
#include "vm.h"

#define STACK_SIZE 32768
#define QUANTUM 25000

// My pthread structure
typedef struct sthread sthread;
struct sthread {
	int id;
	unsigned priority;
	unsigned long born;
	ucontext_t context;
	void *(*function)(void*);
	void *arg;
	void **res;
	int finished;
        LinkedList *waitJoin;
	void *pages;
};
typedef sthread* my_pthread_t;


// Mutex structure
typedef struct lock lock;
struct lock {
	int state;
	LinkedList * wait;
};
typedef lock* my_pthread_mutex_t;

unsigned *nextId;
LinkedList *run; // run queue, processes that want to run and are not waiting for a lock
my_pthread_t *running; // thread currently running (only one cpu)
sthread *mainThread; // store the context of the calling main program
unsigned *nSchedulings; // times that the scheduler has run

// context for the scheduler
// the scheduler will be run swaping this context
ucontext_t signalContext; 
char signalStack[STACK_SIZE];


// alarm and timer
struct sigaction *sa;
struct itimerval *timer;


// Creates a pthread that executes function. Attributes are ignored, arg is not. 
int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);


// Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out and another can be scheduled if one is waiting. 
void my_pthread_yield();


// Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't NULL, any return value from the thread will be saved. 
void my_pthread_exit(void *value_ptr);


//Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.
int my_pthread_join(my_pthread_t thread, void **value_ptr);


//Mutex note: Both the unlock and lock functions should be very fast. If there are any threads that are meant to compete for these functions, my_pthread_yield should be called immediately after running the function in question. Relying on the internal timing will make the function run slower than using yield.

//Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored.
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

 
//Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked.
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

 
// Unlocks a given mutex. 
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);


//Destroys a given mutex. Mutex should be unlocked before doing so.
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);


int getPriority(my_pthread_t *t);
unsigned int getTimeCreated(my_pthread_t *t);
void initOS();

#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#define pthread_yield my_pthread_yield
#endif

#endif
