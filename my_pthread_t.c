#include "my_pthread_t.h"

int my_pthread_create( my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{
  ucontext_t context;
  
  if (!getcontext(&context)) {

    context.uc_stack.ss_sp = malloc(64000);
    context.uc_stack.ss_size = 64000;
    context.uc_stack.ss_flags = 0;

    void *f = function;
    makecontext(&context, f, 0);
    
    setcontext(&context);
  }
}
