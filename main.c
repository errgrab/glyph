/*
 * Glyph Console Emulator
 *
 * Minimal console device for bootstrapping a compiler.
 *
 * Console Device:
 *   'c' console input and output.
 *   'e' stderr output.
 *
 * System:
 *   'X' (88)  - exit:   exit with code
 *
 * Usage: ./glyph <program.glyph> [args...]
 *		./glyph -e "<code>"
 *		echo "input" | ./glyph program.glyph
 */

#define GLYPH_IMPL
#include "glyph.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

/* Device prts */
#define CON_CONSOLE 'c'   /* Read/Write to console */
#define CON_ERROR   'e'   /* Write to stderr */
#define SYS_EXIT	'X'   /* Exit code */
#define MEM_SIZE 0x100

static Glyph vm;

/* Resonance out: handle prt writes */
static void emu_emit(u8 prt) {
	switch (prt) {
	case CON_CONSOLE:
		putchar(vm.p[CON_CONSOLE] & 0xFF);
		fflush(stdout);
		break;
	case CON_ERROR:
		fputc(vm.p[CON_ERROR] & 0xFF, stderr);
		fflush(stderr);
		break;
	case SYS_EXIT:
		exit(vm.p[SYS_EXIT] & 0xFF);
		break;
	}
}

/* Resonance in: handle prt reads */
static void emu_hear(u8 prt) {
	switch (prt) {
	case CON_CONSOLE: {
		int ch = getchar();
		vm.p[CON_CONSOLE] = (ch == EOF) ? 0 : (char)ch;
	} break;
	}
}

/* Load program from file */
static int load_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Error: cannot open '%s'\n", path);
		return -1;
	}
	size_t n = fread(vm.m, 1, MEM_SIZE, f);
	fclose(f);
	if (n == 0) {
		fprintf(stderr, "Error: empty file '%s'\n", path);
		return -1;
	}
	return 0;
}

/* Load program from string */
static void load_string(const char *code) {
	size_t len = strlen(code);
	if (len > MEM_SIZE)
		len = MEM_SIZE;
	memcpy(vm.m, code, len);
}

static void usage(const char *prog) {
	fprintf(stderr, "Glyph Console Emulator\n\n");
	fprintf(stderr, "Usage: %s <program.glyph> [args...]\n", prog);
	fprintf(stderr, "	   %s -e \"<code>\"\n\n", prog);
	fprintf(stderr, "Console Device:\n");
	fprintf(stderr, "  'c' (99)  - read/write: character\n");
	fprintf(stderr, "  'e' (101) - error:  stderr\n");
	fprintf(stderr, "\nSystem:\n");
	fprintf(stderr, "  'X' (88)  - exit:   exit with code\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	/* Initialize VM */
	bzero(&vm, sizeof(vm));
	vm.e = emu_emit;
	vm.h = emu_hear;

	/* Parse arguments */
	if (strcmp(argv[1], "-e") == 0) {
		if (argc < 3) {
			fprintf(stderr, "Error: -e requires code argument\n");
			return 1;
		}
		load_string(argv[2]);
	} else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		usage(argv[0]);
		return 0;
	} else {
		if (load_file(argv[1]) < 0)
			return 1;
	}

	glyph_eval(&vm);
	return 0;
}
