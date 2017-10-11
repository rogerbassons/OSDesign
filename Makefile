all: test test2


test: test.o my_pthread_t.o LinkedList.o
	gcc -m32 test.o my_pthread_t.o LinkedList.o -o test

test2: test2.o my_pthread_t.o LinkedList.o
	gcc -m32 test2.o my_pthread_t.o LinkedList.o -o test2

test.o: test.c
	gcc -m32 -c test.c

test2.o: test2.c
	gcc -m32 -c test2.c

my_pthread_t.o: my_pthread_t.c
	gcc -m32 -c my_pthread_t.c

LinkedList.o: LinkedList.c
	gcc -m32 -c LinkedList.c

clean:
	rm *.o test test2
