/*
 * Glyph Console Emulator
 *
 * Minimal console device for bootstrapping a compiler.
 *
 * Console Device:
 *   'C' (67)  - vector: callback address for input events
 *   'i' (99)  - read:   input character (set before callback)
 *   'o' (111) - write:  write byte to stdout
 *   'e' (101) - error:  write byte to stderr
 *
 * System:
 *   'X' (88)  - exit:   exit with code
 *
 * Input model (like UXN):
 *   1. Program runs to completion
 *   2. For each stdin char: set prt['c'], call vector
 *   3. When stdin exhausted, exit normally
 *
 * Usage: ./glyph <program.glyph> [args...]
 *        ./glyph -e "<code>"
 *        echo "input" | ./glyph program.glyph
 */

#define GLYPH_IMPL
#include "glyph.h"
#include <stdio.h>
#include <stdlib.h>

#define MEM_SIZE 0x40000

/* Device prts */
#define CON_VECTOR  'C'   /* Console input vector */
#define CON_READ    'i'   /* Input character */
#define CON_WRITE   'o'   /* Write to stdout */
#define CON_ERROR   'e'   /* Write to stderr */
#define SYS_EXIT    'X'   /* Exit code */

static Glyph vm;
static u8 mem[MEM_SIZE];

/* Resonance out: handle prt writes */
static void emu_emit(u8 prt) {
    switch (prt) {
    case CON_WRITE:
        putchar(vm.prt[CON_WRITE] & 0xFF);
        fflush(stdout);
        break;
    case CON_ERROR:
        fputc(vm.prt[CON_ERROR] & 0xFF, stderr);
        fflush(stderr);
        break;
    case SYS_EXIT:
        exit(vm.prt[SYS_EXIT] & 0xFF);
        break;
    }
}

/* Resonance in: handle prt reads */
static void emu_sens(u8 prt) {
    switch (prt) {
    case CON_READ: {
        int ch = getchar();
        vm.prt[CON_READ] = (ch == EOF) ? 0 : ch;
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
    size_t n = fread(mem, 1, MEM_SIZE, f);
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
    memcpy(mem, code, len);
}

static void usage(const char *prog) {
    fprintf(stderr, "Glyph Console Emulator\n\n");
    fprintf(stderr, "Usage: %s <program.glyph> [args...]\n", prog);
    fprintf(stderr, "       %s -e \"<code>\"\n\n", prog);
    fprintf(stderr, "Console Device:\n");
    fprintf(stderr, "  'i' (99)  - read:   input character\n");
    fprintf(stderr, "  'o' (111) - write:  stdout\n");
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
    glyph_setup(&vm, mem, MEM_SIZE);
    vm.emit = emu_emit;
    vm.sens = emu_sens;

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
