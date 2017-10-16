#include "my_pthread_t.h"
#include <signal.h>
#include <time.h>

void threadExit(void *res)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

	(*running)->finished = 1;
	if (res != NULL)
		(*running)->res = &res;

	while (!empty((*running)->waitJoin)) {
		pushOrdered(0, run, pop((*running)->waitJoin));
	}

	if (!empty(run)) {
		*running = *pop(run);

		timer.it_value.tv_usec = QUANTUM;

		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		setcontext(&((*running)->context));
	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);

}

void *threadRun(my_pthread_t t)
{
	void **res = t->function(t->arg);
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);
	if (res != NULL) {
		threadExit(*res);
	} else {
		threadExit(NULL);
	}
}

void scheduler()
{

	timer.it_value.tv_usec = QUANTUM;
	nSchedulings += 1;
	if (nSchedulings > 50) {
		orderByOldest(run);
		nSchedulings = 0;
	}

	(*running)->priority += 1;

	if (!empty(run)) {

		my_pthread_t *nextThread = pop(run);
		pushOrdered(0, run, running);
		*running = *nextThread;

		setcontext(&((*running)->context));
	} else {
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
	sigaddset(&signalContext.uc_sigmask, SIGPROF);
	makecontext(&signalContext, scheduler, 0, NULL);


	swapcontext(&(*running)->context, &signalContext);
}

int setMyScheduler()
{
	nSchedulings = 0;

	sa.sa_handler = interrupt;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGPROF);
	sa.sa_flags = 0;	
	sigaction(SIGPROF, &sa, NULL);

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = QUANTUM;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = QUANTUM;

	setitimer(ITIMER_PROF, &timer, NULL);
}

int createNewThread(my_pthread_t * thread, void *(*function) (void *), void *arg)
{
	sthread *t = (sthread *) malloc(sizeof(sthread));
	*thread = t;
	t->waitJoin = (LinkedList *) malloc(sizeof(LinkedList));

	t->function = function;
	t->arg = arg;
	t->finished = 0;
	t->priority = 0;
	t->born = (unsigned long)time(NULL);

	if (getcontext(&(t->context)) != -1) {

		t->context.uc_link = 0;

		sigemptyset(&(t->context.uc_sigmask));

		char *stack = (char *)malloc(STACK_SIZE);
		t->context.uc_stack.ss_sp = stack;
		t->context.uc_stack.ss_size = STACK_SIZE;
		t->context.uc_stack.ss_flags = 0;

		makecontext(&(t->context), (void *)threadRun, 1, t, NULL);
		return 0;

	}
}

int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, 
		void *(*function) (void *), void *arg)
{

	if (run == NULL) {
		run = (LinkedList *) malloc(sizeof(LinkedList));
		mainThread = (sthread *) malloc(sizeof(sthread));
		running = (my_pthread_t *) malloc(sizeof(my_pthread_t));

		nSchedulings = 0;
		*running = mainThread;
		setMyScheduler();
	}
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

	createNewThread(thread, function, arg);
	pushOrdered(0, run, thread);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	return 0;
}

// Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out and another can be scheduled if one is waiting. 
void my_pthread_yield()
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);
	interrupt(0);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);

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

	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

	if (!thread->finished) {
		pushOrdered(0, thread->waitJoin, running);

		if (!empty(run)) {

			ucontext_t *old = &((*running)->context);

			*running = *pop(run);
			
			sigprocmask(SIG_SETMASK, &oldmask, NULL);
			swapcontext(old, &((*running)->context));
		}
	}

	if (value_ptr != NULL)
		*value_ptr = *thread->res;

	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	return 0;
}

//Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored.
int my_pthread_mutex_init(my_pthread_mutex_t * mutex,
		const pthread_mutexattr_t * mutexattr)
{
	lock *l = (lock *) malloc(sizeof(lock));
	(*mutex) = l;
	l->state = 0;
	l->wait = (LinkedList *) malloc(sizeof(LinkedList));
}

int testAndSet(my_pthread_mutex_t * m)
{
	int s = (*m)->state;
	(*m)->state = 1;
	return s;
}

//Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked.
int my_pthread_mutex_lock(my_pthread_mutex_t * mutex)
{

	my_pthread_mutex_t m = *mutex;

	while (testAndSet(mutex) == 1) {
		sigset_t oldmask;
		sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);

		if (m->state == 1) {
			pushOrdered(0, m->wait, running);
			timer.it_value.tv_usec = QUANTUM;

			ucontext_t *old = &((*running)->context);

			*running = *pop(run);

			sigprocmask(SIG_SETMASK, &oldmask, NULL);
			swapcontext(old, &((*running)->context));
		} else {
			sigprocmask(SIG_SETMASK, &oldmask, NULL);
		}
	}
}

// Unlocks a given mutex. 
int my_pthread_mutex_unlock(my_pthread_mutex_t * mutex)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);
	my_pthread_mutex_t m = *mutex;

	if (!empty(m->wait)) {
		pushOrdered(0, run, pop(m->wait));
	}

	m->state = 0;

	sigprocmask(SIG_SETMASK, &oldmask, NULL);

}

//Destroys a given mutex. Mutex should be unlocked before doing so.
int my_pthread_mutex_destroy(my_pthread_mutex_t * mutex)
{
	sigset_t oldmask;
	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldmask);
	my_pthread_mutex_t m = *mutex;
	while (!empty(m->wait)) {
		pushOrdered(0, run, pop(m->wait));
	}
	m->state = 0;
	free(m->wait);
	free(m);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
}


int getPriority(my_pthread_t *t)
{
	return (*t)->priority;
}
unsigned int getTimeCreated(my_pthread_t *t)
{
	return (*t)->born;
}

