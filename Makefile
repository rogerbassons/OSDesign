all: test test2 test3


test: test.o my_pthread_t.o LinkedList.o
	gcc test.o my_pthread_t.o LinkedList.o -o test

test2: test2.o my_pthread_t.o LinkedList.o
	gcc test2.o my_pthread_t.o LinkedList.o -o test2

test3: test3.o my_pthread_t.o LinkedList.o
	gcc test3.o my_pthread_t.o LinkedList.o -o test3


test.o: test.c
	gcc -c test.c

test2.o: test2.c
	gcc -c test2.c

test3.o: test3.c
	gcc -c test3.c


my_pthread_t.o: my_pthread_t.c
	gcc -c my_pthread_t.c

LinkedList.o: LinkedList.c
	gcc -c LinkedList.c

clean:
	rm *.o test test2 test3
