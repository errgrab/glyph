CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

all: example test

example: main.c glyph.h
	$(CC) $(CFLAGS) main.c -o example

test: test.c glyph.h
	$(CC) $(CFLAGS) test.c -o test

clean:
	rm -f example test

.PHONY: all clean
