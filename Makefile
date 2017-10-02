all: test

test: test.o my_pthread_t.o LinkedList.o
	gcc test.o my_pthread_t.o LinkedList.o -o test

test.o: test.c
	gcc -c test.c

my_pthread_t.o: my_pthread_t.c
	gcc -c my_pthread_t.c

LinkedList.o: LinkedList.c
	gcc -c LinkedList.c

clean:
	rm *.o test
