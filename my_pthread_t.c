#include "my_pthread_t.h"
#include <signal.h>
void scheduler() {
	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);
	while (empty(&run));
	if (!empty(&run)) {
		my_pthread_t *nextThread = pop(&run);
		ucontext_t *nextContext = &(nextThread->context);
		printf("RUN DEQUEUED: %p\n", nextThread); 
		
		ucontext_t *oldContext = &(running->context);
		push(&run, running);
		running = nextThread;
		print(&run);

	      

		sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);

		setcontext(&(running->context));
		
	}
}

int setMyScheduler()
{
	my_pthread_t t;
	running = &t;

	
	sa.sa_handler = scheduler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER;
	sigaction(SIGPROF, &sa, NULL);
 
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 25000;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 25000;
  
	setitimer(ITIMER_PROF, &timer, NULL);
}

int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{

	
	if (running == NULL) {
		
		if (getcontext(&schedulerContext) != -1) {

				schedulerContext.uc_link = 0;
				char stack[16384];
				schedulerContext.uc_stack.ss_sp = stack;
				schedulerContext.uc_stack.ss_size = sizeof(stack);
				schedulerContext.uc_stack.ss_flags = 0;
		
				makecontext(&schedulerContext, scheduler, 0);
		
		}
	}

	

	ucontext_t *c = &(thread->context);
	if (getcontext(c) != -1) {

		

		c->uc_link = &schedulerContext;
		char stack[16384];
		c->uc_stack.ss_sp = stack;
		c->uc_stack.ss_size = sizeof(stack);
		c->uc_stack.ss_flags = 0;
		
		void *f = function;
		makecontext(c, f, 0);

		push(&run, thread);
		
		
		
	}


	
	printf("RUN QUEUED: %p\n", thread); 

	if (sa.sa_handler == NULL) {
		setMyScheduler();
	}

    
	
}


