#include "my_pthread_t.h"
#include <signal.h>

void my_scheduler (int signum)
{
	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);

	if (!empty(&run)) {
		printf("WHY\n"); //DEBUG
		my_thread *t = pop(&run);
		if (getcontext(&context) != -1) {
			t->context.uc_link = 0;
			t->context.uc_stack.ss_sp = malloc(64000);
			t->context.uc_stack.ss_size = 64000;
			t->context.uc_stack.ss_flags = 0;

			void *f = t->function;
			makecontext(&context, f, 0);

			ucontext_t *old = &(running->context);
			push(&run, running);
			running = t;
			
			swapcontext(old,running);
			print(&run); //DEBUG
    
		
		}
	}

	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
}

int setMyScheduler()
{
	sa.sa_handler = &my_scheduler;
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


	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);

	my_pthread t;
	t.function = function;
	thread = &t;

	push(&run, &thread);

	if (sa.sa_handler == NULL) {
		setMyScheduler();
	}
    
	
	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
}
