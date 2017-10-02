#include "my_pthread_t.h"
#include <signal.h>

void my_scheduler (int signum)
{
	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);

	if (!empty(&run)) {
		printf("WHY\n"); //DEBUG
		ucontext_t c = pop(&run);


		setcontext(&c);
	}

	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
}

int myScheduler()
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
	ucontext_t context;
	ucontext_t old;
    
	if (getcontext(&context) != -1) {
		context.uc_link = 0;
		context.uc_stack.ss_sp = malloc(64000);
		context.uc_stack.ss_size = 64000;
		context.uc_stack.ss_flags = 0;

		void *f = function;
		makecontext(&context, f, 0);

		thread = (my_pthread_t*) nelements(&run);
		push(&run, &context);
		print(&run); //DEBUG
    
		if (sa.sa_handler == NULL) {
			myScheduler();
		}
	}
	sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
}
