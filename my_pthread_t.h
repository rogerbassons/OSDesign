#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <sys/time.h>
#include "LinkedList.h"


typedef struct my_pthread_t my_pthread_t;
struct my_pthread_t {
	ucontext_t context;
	void *(*function)(void*);
};

my_pthread_t *running;
ucontext_t schedulerContext;

LinkedList run;

struct sigaction sa;
struct itimerval timer;


/* Creates a pthread that executes function. Attributes are ignored, arg is not. */
int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

 


/*
void my_pthread_yield();
// Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out and another can be scheduled if one is waiting. 

 

void pthread_exit(void *value_ptr);
// Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't NULL, any return value from the thread will be saved. 

 

int my_pthread_join(my_pthread_t thread, void **value_ptr);

//Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.

 

//Mutex note: Both the unlock and lock functions should be very fast. If there are any threads that are meant to compete for these functions, my_pthread_yield should be called immediately after running the function in question. Relying on the internal timing will make the function run slower than using yield.

 

int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

//Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored.

 

int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);
//Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked.

 

int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);
// Unlocks a given mutex. 

 

int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);
//Destroys a given mutex. Mutex should be unlocked before doing so.

*/

#endif
