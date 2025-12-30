/*
 * GLYPH - Single-header character-based VM (~100 lines)
 * Usage: #define GLYPH_IMPL before including in ONE .c file
 */
#ifndef GLYPH_H
#define GLYPH_H

#include <stdint.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    uint8_t  *mem;
    uint32_t  size;
    uint32_t  reg[128];
    uint32_t  port[256];
    int       halt;
} Glyph;

#define glyph_pc(vm)  ((vm)->reg['.'])
#define glyph_sp(vm)  ((vm)->reg[','])

void glyph_init(Glyph *vm, uint8_t *mem, uint32_t size);
void glyph_run(Glyph *vm);

/* ─────────────────────────────────────────────────────────────────────────── */
#ifdef GLYPH_IMPL

#define R(x)  vm->reg[(x) & 127]
#define M(x)  vm->mem[(x) & (vm->size - 1)]
#define PC    R('.')

static uint8_t fetch(Glyph *vm) {
    while (PC < vm->size && isspace(vm->mem[PC])) PC++;
    return (PC < vm->size) ? vm->mem[PC++] : (vm->halt = 1, 0);
}

void glyph_run(Glyph *vm) {
    uint8_t op, a, b, c;
    while (!vm->halt) {
        op = fetch(vm);
        if (vm->halt) break;

        switch (op) {
        /* Arithmetic: +abc -abc *abc /abc %abc */
        case '+': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) + R(c); break;
        case '-': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) - R(c); break;
        case '*': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) * R(c); break;
        case '/': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(c) ? R(b)/R(c) : 0; break;
        case '%': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(c) ? R(b)%R(c) : 0; break;

        /* Bitwise: &abc |abc ^abc ~ab <abc >abc */
        case '&': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) & R(c); break;
        case '|': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) | R(c); break;
        case '^': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) ^ R(c); break;
        case '~': a=fetch(vm); b=fetch(vm); R(a) = ~R(b); break;
        case '<': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) << R(c); break;
        case '>': a=fetch(vm); b=fetch(vm); c=fetch(vm); R(a) = R(b) >> R(c); break;

        /* Load: :agX :axF :a.b */
        case ':':
            a = fetch(vm); b = fetch(vm);
            if      (b == 'g') R(a) = fetch(vm);
            else if (b == 'x')  { c = fetch(vm); R(a) = (c <= '9') ? c - '0' : (c | 32) - 'a' + 10; }
            else if (b == '.')  R(a) = R(fetch(vm));
            break;

        /* Memory: @ab !ab */
        case '@': a=fetch(vm); b=fetch(vm); R(a) = M(R(b)); break;
        case '!': a=fetch(vm); b=fetch(vm); M(R(a)) = R(b); break;

        /* Ports: (ab )ab */
        case '(': a=fetch(vm); b=fetch(vm); R(a) = vm->port[R(b) & 255]; break;
        case ')': a=fetch(vm); b=fetch(vm); vm->port[R(a) & 255] = R(b); break;

        /* Control: .a ?=ab ;a , */
        case '.': a=fetch(vm); PC = R(a); break;
        case '?':
            a=fetch(vm); b=fetch(vm); c=fetch(vm);
            if ((a=='=' && R(b)!=R(c)) || (a=='!' && R(b)==R(c)) ||
                (a=='>' && R(b)<=R(c)) || (a=='<' && R(b)>=R(c))) PC++;
            break;
        case ';':
            a = fetch(vm);
            glyph_sp(vm) -= 4;
            M(glyph_sp(vm)+0) = PC; M(glyph_sp(vm)+1) = PC >> 8;
            M(glyph_sp(vm)+2) = PC >> 16; M(glyph_sp(vm)+3) = PC >> 24;
            PC = R(a);
            break;
        case ',':
            PC = M(glyph_sp(vm)) | (M(glyph_sp(vm)+1)<<8) |
                 (M(glyph_sp(vm)+2)<<16) | (M(glyph_sp(vm)+3)<<24);
            glyph_sp(vm) += 4;
            break;

        case 0: vm->halt = 1; break;
        default: break;
        }
    }
}

void glyph_init(Glyph *vm, uint8_t *mem, uint32_t size) {
    memset(vm, 0, sizeof(Glyph));
    vm->mem = mem;
    vm->size = size;
}

#undef R
#undef M
#undef PC
#endif /* GLYPH_IMPL */
#endif /* GLYPH_H */
