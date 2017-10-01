all: test

test: test.o my_pthread_t.o DynamicContextArray.o
	gcc test.o my_pthread_t.o DynamicContextArray.o -o test

test.o: test.c
	gcc -c test.c

my_pthread_t.o: my_pthread_t.c
	gcc -c my_pthread_t.c

DynamicContextArray.o: DynamicContextArray.c
	gcc -c DynamicContextArray.c

clean:
	rm *.o test
