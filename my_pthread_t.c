#include "my_pthread_t.h"
#include <signal.h>

// the scheduler
void scheduler(int signum) {
	
	//check for threads at run queue
	if (!empty(&run)) {
		
		my_pthread_t *nextThread = pop(&run);
		ucontext_t *nextContext = &(nextThread->context);
		
		ucontext_t *oldContext = NULL;
		if (running != NULL)
			oldContext = &(running->context);
                else {
			// only runs once at the beginning
			oldContext = &(mainThread.context);
			running = &mainThread;
		}


		push(&run, running);
		
		running = nextThread;
		
		
		if (oldContext != NULL)
			swapcontext(oldContext, &(running->context));
		
	}
}

// sets the periodic signal to run scheduler
int setMyScheduler()
{
	
	sa.sa_handler = scheduler;
	sigemptyset(&sa.sa_mask);
	sigaddset (&sa.sa_mask, SIGALRM);
	sa.sa_flags = 0;//SA_RESTART | SA_SIGINFO;
	sigaction(SIGALRM, &sa, NULL);
 
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 250;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 250;
  
	setitimer(ITIMER_REAL, &timer, NULL);
}

void createNewSchedulerContext() {


	if (getcontext(&schedulerContext) != -1) {

		schedulerContext.uc_link = 0;
		
		sigemptyset(&(schedulerContext.uc_sigmask));
		sigaddset(&(schedulerContext.uc_sigmask), SIGALRM);

		char stack[16384];
		schedulerContext.uc_stack.ss_sp = stack;
		schedulerContext.uc_stack.ss_size = sizeof(stack);
		schedulerContext.uc_stack.ss_flags = 0;

		makecontext(&schedulerContext, scheduler, 0);

	}

}

int createNewThread(my_pthread_t *thread, void *(*function)(void*))
{
// new thread construction
	ucontext_t *c = &(thread->context);
	if (getcontext(c) != -1) {

		c->uc_link = &schedulerContext; // if thread finishes running call the scheduler
		
		sigemptyset(&c->uc_sigmask);
		
		char stack[16384];
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
		createNewSchedulerContext();	

		setMyScheduler();
	}

	return 0;
}


