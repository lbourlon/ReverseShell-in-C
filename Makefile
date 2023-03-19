CC=gcc
CFLAGS=-lm
#DEPS = hellomake.h

main: *.c# $(DEPS)
	$(CC) -o $@ $< $(CFLAGS)