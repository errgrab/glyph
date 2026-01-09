CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

all: glyph test

glyph: main.c glyph.h
	$(CC) $(CFLAGS) main.c -o glyph

test: test.c glyph.h
	$(CC) $(CFLAGS) test.c -o test

clean:
	rm -f glyph test
.PHONY: all clean
