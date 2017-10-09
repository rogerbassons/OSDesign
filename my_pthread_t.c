#include "my_pthread_t.h"
#include <signal.h>

void threadExit(void *res)
{
	sigset_t oldmask;
	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
	}

	running->finished = 1;
	if (res != NULL)
		running->res = res;
	
	if (!empty(&run)) {
		
		running = pop(&run);
		
		//printf("exit to: %p\n---------------\n", running);
		setcontext(&(running->context));	
	}
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
	}


}

void *threadRun(my_pthread_t t)
{
	void **res = t->function(t->arg);
	//printf("DONE!\n");
	if(res != NULL){
		threadExit(*res);
	} else {
		threadExit(NULL);
	}
}



// the scheduler
void scheduler() {
	//check for threads at run queue
	
	//printf("scheduling...\n");
	if (!empty(&run)) {
		
		my_pthread_t nextThread = pop(&run);
		
		push(&run, running);
		running = nextThread;
		
		//printf("new: %p\n---------------\n", running);
		setcontext(&(running->context));	
	}
}

void interrupt(int signum) 
{
	getcontext(&signalContext);
	signalContext.uc_stack.ss_sp = signalStack;
	signalContext.uc_stack.ss_size = STACK_SIZE;
	signalContext.uc_stack.ss_flags = 0;
	sigemptyset(&signalContext.uc_sigmask);
	sigaddset(&signalContext.uc_sigmask, SIGALRM);
	makecontext(&signalContext, scheduler, 0, NULL);
	
        ucontext_t *oldContext = NULL;
	if (running != NULL)
		oldContext = &(running->context);
	else {
		// only runs once at the beginning
		oldContext = &(mainThread.context);
		running = &mainThread;
	}

	//printf("old: %p\n", oldContext);

	swapcontext(oldContext,&signalContext);
}

// sets the periodic signal to run scheduler
int setMyScheduler()
{
	
	sa.sa_handler = interrupt;
	sigemptyset(&sa.sa_mask);
	sigaddset (&sa.sa_mask, SIGALRM);
	sa.sa_flags = 0; //SA_RESTART | SA_SIGINFO;
	sigaction(SIGALRM, &sa, NULL);
 
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = QUANTUM;
  
	setitimer(ITIMER_REAL, &timer, NULL);
}


int createNewThread(my_pthread_t *thread, void *(*function)(void*), void *arg)
{
	// new thread construction
	sthread *t = (sthread *) malloc(sizeof(sthread));
	*thread = t;
	t->function = function;
	t->arg = arg;
	t->finished = 0;
	ucontext_t *c = &(t->context);
	if (getcontext(c) != -1) {

		c->uc_link = 0; 
		
		sigemptyset(&c->uc_sigmask);
		
		char stack[STACK_SIZE];
		c->uc_stack.ss_sp = stack;
		c->uc_stack.ss_size = sizeof(stack);
		c->uc_stack.ss_flags = 0;
		
		makecontext(c, (void *)threadRun, 1, t, NULL);
		return 0;
		
	}
}

int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{
	
	sigset_t oldmask;
	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
		return 1;
	}

	// only executed once (at the beginning)
	// constructs the scheduler context and starts the periodic signal
	
	createNewThread(thread, function, arg);
	push(&run, *thread);
 
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
		return 1;
	}


	if (running == NULL) {
		setMyScheduler();
	}

	return 0;
}

// Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out and another can be scheduled if one is waiting. 
void my_pthread_yield()
{
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = QUANTUM;
  
	setitimer(ITIMER_REAL, &timer, NULL);

}

// Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't NULL, any return value from the thread will be saved. 
void pthread_exit(void *value_ptr) 
{
	threadExit(value_ptr);
}

//Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.
int my_pthread_join(my_pthread_t thread, void **value_ptr)
{
	
	sigset_t oldmask;

	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
		return 1;
	}
		

	value_ptr = thread->res;

	while(!thread->finished)
		interrupt(0);
 
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
		return 1;
	}
	
	return 0;
}


