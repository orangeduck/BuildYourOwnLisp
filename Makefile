CC = gcc
CFLAGS = -std=c99 -Wall -Werror -Wno-unused -g


all: example1 example2 example2a example2b example3 example4 example5 example6 example7 example8 example9 example10

example1: example1.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example2: example2.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example2a: example2a.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
 
example2b: example2b.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example3: example3.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example4: example4.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example5: example5.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example6: example6.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example7: example7.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example8: example8.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example9: example9.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
  
example10: example10.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ -lm -ledit -o $@
