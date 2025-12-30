/*
 * GLYPH - Single-header character-based VM (~100 lines)
 * Usage: #define GLYPH_IMPL before including in ONE .c file
 */
#ifndef GLYPH_H
#define GLYPH_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef uint8_t  u8;
typedef uint32_t u32;

typedef struct {
    u8 *mem;
	u8  sp;
    u32 size;
    u32 reg[128];
	u32 stk[256];
    u32 port[256];
    bool halt;
} Glyph;

void glyph_init(Glyph *vm, u8 *mem, u32 size);
void glyph_run(Glyph *vm);

/* ────────────────────────────────────────────────────────────────────────── */
#ifdef GLYPH_IMPL

#define R(x)  vm->reg[(x) & 127]
#define M(x)  vm->mem[(x) & (vm->size - 1)]
#define PC    R('.')
#define SP    R(',')

static inline u8 N(Glyph *vm) {
    return (PC < vm->size) ? vm->mem[PC++] : (vm->halt = 1, 0);
}

#include <stdio.h>
void glyph_run(Glyph *vm) {
    u8 op, a, b, c;
    while (!vm->halt) {
        op = N(vm);
        if (vm->halt) break;

        switch (op) {
        /* Arithmetic: +abc -abc *abc /abc %abc */
        case '+': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) + R(c); break;
        case '-': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) - R(c); break;
        case '*': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) * R(c); break;
        case '/': a=N(vm); b=N(vm); c=N(vm); R(a) = R(c) ? R(b)/R(c) : 0; break;
        case '%': a=N(vm); b=N(vm); c=N(vm); R(a) = R(c) ? R(b)%R(c) : 0; break;

        /* Bitwise: &abc |abc ^abc ~ab <abc >abc */
        case '&': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) & R(c); break;
        case '|': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) | R(c); break;
        case '^': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) ^ R(c); break;
        case '~': a=N(vm); b=N(vm); R(a) = ~R(b); break;
        case '<': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) << R(c); break;
        case '>': a=N(vm); b=N(vm); c=N(vm); R(a) = R(b) >> R(c); break;

        /* Load: :agX :axF :a.b */
        case ':':
            a = N(vm); b = N(vm);
            if      (b == 'g') R(a) = N(vm);
            else if (b == 'x') { c = N(vm); R(a) = (c <= '9') ? c - '0' : (c | 32) - 'a' + 10; }
            else if (b == '.') R(a) = R(N(vm));
            break;

        /* Memory: @ab !ab */
        case '@': a=N(vm); b=N(vm); R(a) = M(R(b)); break;
        case '!': a=N(vm); b=N(vm); M(R(a)) = R(b); break;

        /* Ports: #<ab #>ab */
        case '#':
            a=N(vm); b=N(vm); c=N(vm);
            if      (a == '<') R(b) = vm->port[R(c) & 255];
            else if (a == '>') vm->port[R(b) & 255] = R(c);
            break;

        /* Control: .a ?=ab ;a , */
        case '.': a=N(vm); PC = R(a); break;
        case '?':
            a=N(vm); b=N(vm); c=N(vm);
            if ((a=='=' && R(b)!=R(c)) || (a=='!' && R(b)==R(c)) ||
                (a=='>' && R(b)<=R(c)) || (a=='<' && R(b)>=R(c))) PC++;
            break;
        case ';':
            a = N(vm);
            SP -= 4;
            M(SP+0) = PC; M(SP+1) = PC >> 8;
            M(SP+2) = PC >> 16; M(SP+3) = PC >> 24;
            PC = R(a);
            break;
        case ',':
            PC = M(SP) | (M(SP+1)<<8) |
                 (M(SP+2)<<16) | (M(SP+3)<<24);
            SP += 4;
            break;
        case 0: vm->halt = 1; break;
		case ' ':
		case '\f':
		case '\n':
		case '\v':
		case '\r':
		case '\t': break;
        default: vm->halt = 1; break;
        }
    }
}

void glyph_init(Glyph *vm, u8 *mem, u32 size) {
    memset(vm, 0, sizeof(Glyph));
    vm->mem = mem;
    vm->size = size;
	vm->reg[','] = size;
}

#undef R
#undef M
#undef PC
#endif /* GLYPH_IMPL */
#endif /* GLYPH_H */
