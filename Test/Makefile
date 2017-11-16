CC = gcc
CFLAGS = -g -w

all:: test test2 test3 test4 test5 test6

test: 
	$(CC) $(CFLAGS) -pthread -o test test.c -L../ -lmy_pthread

test2: 
	$(CC) $(CFLAGS) -pthread -o test2 test2.c -L../ -lmy_pthread

test3: 
	$(CC) $(CFLAGS) -pthread -o test3  test3.c -L../ -lmy_pthread

test4: 
	$(CC) $(CFLAGS) -pthread -o test4  test4.c -L../ -lmy_pthread

test5: 
	$(CC) $(CFLAGS) -pthread -o test5  test5.c -L../ -lmy_pthread

test6: 
	$(CC) $(CFLAGS) -pthread -o test6  test6.c -L../ -lmy_pthread

clean:
	rm -rf test test2 test3 test4 test5 test6 *.o ./record/
