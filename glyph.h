/* GLYPH - Single-header character-based VM (~200 lines)
 * Usage: #define GLYPH_IMPL before including in ONE .c file
 */
#ifndef GLYPH_H
#define GLYPH_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
#define SIZE 0x100

/* Resonance */
typedef void (*R)(u8 p);

typedef struct {
	u8 m[SIZE], r[SIZE], s[SIZE], p[SIZE];
	u8 T;
	R e, h;
	bool halt;
} Glyph;

void glyph_read(Glyph *vm, char *book);
void glyph_eval(Glyph *vm);

/* ────────────────────────────────────────────────────────────────────────── */
#ifdef GLYPH_IMPL

static inline u8 glyph_getr(Glyph *vm, u8 reg) {
	if (reg == ',') {
		return vm->s[--vm->T];
	}
	return vm->r[reg];
}

static inline void glyph_setr(Glyph *vm, u8 reg, u8 val) {
	if (reg == ',') {
		vm->s[vm->T++] = val;
		return;
	}
	vm->r[reg] = val;
}

static inline u8 glyph_next(Glyph *vm) {
	u8 pc = glyph_getr(vm, '.');
	u8 res = vm->m[pc++];
	glyph_setr(vm, '.', pc);
	return res;
}

#define R(x) glyph_getr(vm, (x))
#define WR(x, v) glyph_setr(vm, (x), (v))
#define M(x) vm->m[(x)]
#define P(x) vm->p[(x)]
#define ACC(x) glyph_setr(vm, '=', (x))
#define FLG(x) glyph_setr(vm, '?', (x))
#define A R('=')
#define N  glyph_next(vm)

void glyph_eval(Glyph *vm) {
	u8 op, a, b;
	while (!vm->halt) {
		op = N;
//		printf("op: %c pc: %d acc: %d flg: %d\n", op, R('.'), A, R('?'));
		if (vm->halt) break;
		switch (op) {
		/* NooP */
		case' ':case'\f':case'\n':case'\v':case'\r':case'\t':
			ACC(0); break; /* NesCafe Aproved */
		/*IMM*/
		case'0':case'1':case'2':case'3':case'4':
		case'5':case'6':case'7':case'8':case'9':
			ACC((A * 10) + (op - '0')); break;
		case '=': WR(N, A); ACC(0); break;
		case '\'': ACC(N); break;
		/* Arithmetic: +abc -abc *abc /abc %abc */
		case '+': a=R(N);b=R(N); ACC(a + b); break;
		case '-': a=R(N);b=R(N); ACC(a - b); break;
		case '*': a=R(N);b=R(N); ACC(a * b); break;
		case '/': a=R(N);b=R(N); ACC(b ? a / b : 0); break;
		case '%': a=R(N);b=R(N); ACC(b ? a % b : 0); break;
		/* Bitwise: &abc |abc ^abc ~ab <bc >abc */
		case '&': a=R(N);b=R(N); ACC(a & b); break;
		case '|': a=R(N);b=R(N); ACC(a | b); break;
		case '^': a=R(N);b=R(N); ACC(a ^ b); break;
		case '<': a=R(N); ACC(a << A); break;
		case '>': a=R(N); ACC(a >> A); break;
		case '~': a=R(N); ACC(~a); break;
		/* Memory: @<a @>a */
		case '@': { a=N;b=N;
			switch (a) {
			case '<': WR(b, M(A)); break;
			case '>': M(A) = R(b); break;
			}
		} break;
		/* Ports: #<a #>a (resonance) */
		case '#': { a=N;b=N;
			switch (a) {
			case '<': if (vm->h) vm->h(A); WR(b, P(A)); break;
			case '>': P(A) = R(b); if (vm->e) vm->e(A); break;
			}
		} break;
		/* Compare: ?=a ?!a ?<a ?>a */
		case '?': { a=N;b=N;
			switch (a) {
			case '=': FLG(A == R(b)); break;
			case '!': FLG(A != R(b)); break;
			case '<': FLG(A <  R(b)); break;
			case '>': FLG(A >  R(b)); break;
			}
		} break;
		/* Conditional Move: :a (if ? is true move from acc to a) */
		case ':': if (R('?')) WR(N, A); ACC(0); break;
		/* Call: ;a */
		case ';': vm->s[vm->T++] = R('.'); WR('.', R(N)); break;
		case '`': case 0: vm->halt = 1; break;
		default: a=N; WR(a, R(op)); break;
		}
	}
}

#undef R
#undef M
#undef ACC
#undef FLG
#undef N

#endif /* GLYPH_IMPL */

#endif /* GLYPH_H */
