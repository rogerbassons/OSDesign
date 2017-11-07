CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib


Target: my_pthread.a

my_pthread.a: my_pthread.o LinkedList.o vm.o
	$(AR) libmy_pthread.a my_pthread.o LinkedList.o vm.o
	$(RANLIB) libmy_pthread.a

my_pthread.o: my_pthread_t.h
	$(CC) -pthread $(CFLAGS) my_pthread.c

LinkedList.o: LinkedList.h
	$(CC) $(CFLAGS) LinkedList.c

vm.o: vm.h
	$(CC) $(CFLAGS) vm.c

clean:
	rm -rf testfile *.o *.a
