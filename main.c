/* Example: glyph VM usage */
#define GLYPH_IMPL
#include "glyph.h"
#include <stdio.h>

int main(void) {
    uint8_t mem[256];
    Glyph vm;

    glyph_init(&vm, mem, sizeof(mem));

    /* Load program: a=5, b=3, c=a+b */
    uint8_t prog[] = ":ax5 :bx3 +cab";
    memcpy(mem, prog, sizeof(prog));

    glyph_run(&vm);

    printf("5 + 3 = %u\n", vm.reg['c']);
    return 0;
}
