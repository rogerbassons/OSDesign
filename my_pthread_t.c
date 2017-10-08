#include "my_pthread_t.h"
#include <signal.h>



// the scheduler
void scheduler() {
	//check for threads at run queue
	if (!empty(&run)) {
		
		my_pthread_t *nextThread = pop(&run);
		
		push(&run, running);
		
		running = nextThread;
		
		
		setcontext(&(running->context));
		
	}
}

void interrupt(int signum) {
	getcontext(&signalContext);
	signalContext.uc_stack.ss_sp = signalStack;
	signalContext.uc_stack.ss_size = STACK_SIZE;
	signalContext.uc_stack.ss_flags = 0;
	sigemptyset(&signalContext.uc_sigmask);
	sigaddset(&signalContext.uc_sigmask, SIGALRM);
	makecontext(&signalContext, scheduler, 1);
	
        ucontext_t *oldContext = NULL;
	if (running != NULL)
		oldContext = &(running->context);
	else {
		// only runs once at the beginning
		oldContext = &(mainThread.context);
		running = &mainThread;
	}

	swapcontext(oldContext,&signalContext);
}

// sets the periodic signal to run scheduler
int setMyScheduler()
{
	
	sa.sa_handler = interrupt;
	sigemptyset(&sa.sa_mask);
	sigaddset (&sa.sa_mask, SIGALRM);
	sa.sa_flags = 0;//SA_RESTART | SA_SIGINFO;
	sigaction(SIGALRM, &sa, NULL);
 
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = QUANTUM;
  
	setitimer(ITIMER_REAL, &timer, NULL);
}


int createNewThread(my_pthread_t *thread, void *(*function)(void*))
{
// new thread construction
	ucontext_t *c = &(thread->context);
	if (getcontext(c) != -1) {

		c->uc_link = 0; // if thread finishes running call the scheduler
		
		sigemptyset(&c->uc_sigmask);
		
		char stack[STACK_SIZE];
		c->uc_stack.ss_sp = stack;
		c->uc_stack.ss_size = sizeof(stack);
		c->uc_stack.ss_flags = 0;
		
		void *f = function;
		makecontext(c, f, 0);
		return 0;
		
	}
}

int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{

	createNewThread(thread, function);

	sigset_t oldmask;
	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
		return 1;
	}

	push(&run, thread);
 
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
		return 1;
	}



	// only executed once (at the beginning)
	// constructs the scheduler context and starts the periodic signal
	if (running == NULL) {
		setMyScheduler();
	}

	return 0;
}


