#include "my_pthread_t.h"
#include <signal.h>

void threadExit(void *res)
{
	sigset_t oldmask;
	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
	}
	//printf("exit from : %p\n", (*running));
	//printf("%p\n", run);
	print(run);

	(*running)->finished = 1;
	if (res != NULL)
		(*running)->res = &res;

	while (!empty((*running)->waitJoin)) {
		//printf("wake up: ");
		push(run, pop((*running)->waitJoin));
		print(run);
	}
	
	if (!empty(run)) {
		print(run);
		//printf("exit thread, wake up next: ");
		*running = *pop(run);
		
		//printf("exit to: %p\n---------------\n", *running);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		setcontext(&((*running)->context));	
	}
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
	}


}

void *threadRun(my_pthread_t t)
{
	void **res = t->function(t->arg);
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask); 
	if(res != NULL){
		threadExit(*res);
	} else {
		threadExit(NULL);
	}

	sigprocmask(SIG_SETMASK, &oldmask, NULL); 
	//printf("Exit Exit, have a nice jump...");
}



// the scheduler
void scheduler() {
	//check for threads at run queue
	
	//printf("scheduling...\n");
	if (!empty(run)) {
		
		my_pthread_t *nextThread = pop(run);
		//printf("saving thread: %p   -> ", *running);
		push(run, running);
		print(run);
		*running = *nextThread;
		
		//printf("setcontext scheduler: %p\n---------------\n", *running);
		setcontext(&((*running)->context));	
	} else {
		//printf("resuming scheduler: %p\n---------------\n", *running);
		setcontext(&((*running)->context));
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
	

	//printf("interrupt old: %p\n", *running);

	swapcontext(&(*running)->context,&signalContext);
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
	t->waitJoin = (LinkedList *) malloc(sizeof(LinkedList));

	(threads[*nThreads]) =  *thread;
	t->id = *nThreads;
	*nThreads += 1;

	t->function = function;
	t->arg = arg;
	t->finished = 0;
	if (getcontext(&(t->context)) != -1) {

		t->context.uc_link = 0; 
		
		sigemptyset(&(t->context.uc_sigmask));
		
		char *stack = (char*) malloc(sizeof(STACK_SIZE));
		t->context.uc_stack.ss_sp = stack;
		t->context.uc_stack.ss_size = STACK_SIZE;
		t->context.uc_stack.ss_flags = 0;
		
		makecontext(&(t->context), (void *)threadRun, 1, t, NULL);
		return 0;
		
	}
}

int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{

	if (run == NULL) {
		threads = (my_pthread_t *)malloc(sizeof(my_pthread_t)*50);
		nThreads = (int *)malloc(sizeof(int)); 
		run = (LinkedList *) malloc(sizeof(LinkedList));
		wait = (LinkedList *) malloc(sizeof(LinkedList));
		mainThread = (sthread *) malloc(sizeof(sthread));
		running = (my_pthread_t *) malloc(sizeof(my_pthread_t));

		*running = mainThread;
		*nThreads = 0;
		setMyScheduler();
	}
	sigset_t oldmask;
	if (sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask) < 0) {
		perror ("sigprocmask");
		return 1;
	}

		
	createNewThread(thread, function, arg);
	//printf("Create: ->");
	push(run, thread);
	print(run);


 
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
		return 1;
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
	
	/*while(1) {
		sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask); 
		printf("%i\n", thread->finished);
		printf("thread: %p\n", thread);
		printf("main: %p\n", (*running));
		sigprocmask(SIG_SETMASK, &oldmask, NULL); 
		int i = 0;
	}*/
	
	if (!thread->finished) {
		//printf("wait: ");
		push(thread->waitJoin, running);
		print(run);
		
		if (!empty(run)) {
		
			ucontext_t *old = &((*running)->context);
		
			print(run);

			//printf("wake up next: ");
			*running = *pop(run);
			print(run);
			//printf("%p\n", run);

			//printf("new: %p\n---------------\n", *running);
	sigprocmask(SIG_SETMASK, &oldmask, NULL); 
			swapcontext(old, &((*running)->context));	
		}
	}

		


	if (value_ptr != NULL) 
		*value_ptr = *thread->res;

 
	if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {
		perror ("sigprocmask");
		return 1;
	}
	
	return 0;
}


