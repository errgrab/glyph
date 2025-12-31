CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

all: glyph test glyph-addr glyph-dis gen-glyph-addr gen-forth

glyph: main.c glyph.h
	$(CC) $(CFLAGS) main.c -o glyph

test: test.c glyph.h
	$(CC) $(CFLAGS) test.c -o test

glyph-addr: tools/glyph-addr.c
	$(CC) $(CFLAGS) tools/glyph-addr.c -o glyph-addr

glyph-dis: tools/glyph-dis.c
	$(CC) $(CFLAGS) tools/glyph-dis.c -o glyph-dis

gen-glyph-addr: tools/gen-glyph-addr.c tools/glyphc.h
	$(CC) $(CFLAGS) tools/gen-glyph-addr.c -o gen-glyph-addr

gen-forth: tools/gen-forth.c tools/glyphc.h
	$(CC) $(CFLAGS) tools/gen-forth.c -o gen-forth

clean:
	rm -f glyph test glyph-addr glyph-dis gen-glyph-addr gen-forth

.PHONY: all clean
