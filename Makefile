CC=gcc
CFLAGS=-Wall -I/usr/include/lua5.4
LDFLAGS=-llua5.4 -lm -ldl

all: engine

engine: main.c
	$(CC) $(CFLAGS) -o engine main.c $(LDFLAGS)

clean:
	rm -f engine
