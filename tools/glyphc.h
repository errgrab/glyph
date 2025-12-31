/*
 * glyphc.h - Glyph Assembler Library
 * 
 * A C library for generating Glyph bytecode with automatic label resolution.
 * 
 * Usage:
 *   GlyphAsm g;
 *   glyph_init_asm(&g, buffer, sizeof(buffer));
 *   
 *   // Emit instructions
 *   G_LOAD_HEX(&g, 'a', 5);      // :ax5
 *   G_LOAD_LIT(&g, 'b', 'H');    // :bgH
 *   G_ADD(&g, 'c', 'a', 'b');    // +cab
 *   
 *   // Labels
 *   G_LABEL(&g, "loop");
 *   G_READ_PORT(&g, 'v', 'p');   // #<vp
 *   G_JUMP_LABEL(&g, "loop");    // Emits code to jump to "loop"
 *   
 *   glyph_resolve(&g);           // Fix up label addresses
 *   glyph_write(&g, "out.glyph");
 */

#ifndef GLYPHC_H
#define GLYPHC_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define GLYPH_BASE    0x0100
#define GLYPH_MAX_LABELS 128
#define GLYPH_MAX_REFS   256

typedef struct {
    char name[32];
    uint32_t addr;
} GlyphLabel;

typedef struct {
    char name[32];
    uint32_t addr;      /* Address where the 16-bit value should go */
    char reg;           /* Register to load the address into */
} GlyphLabelRef;

typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t pos;
    
    GlyphLabel labels[GLYPH_MAX_LABELS];
    int label_count;
    
    GlyphLabelRef refs[GLYPH_MAX_REFS];
    int ref_count;
} GlyphAsm;

/* Initialize assembler */
static inline void glyph_init_asm(GlyphAsm *g, uint8_t *buf, uint32_t size) {
    memset(g, 0, sizeof(*g));
    g->buf = buf;
    g->size = size;
    g->pos = 0;
}

/* Emit a single byte */
static inline void G_EMIT(GlyphAsm *g, uint8_t b) {
    if (g->pos < g->size) g->buf[g->pos++] = b;
}

/* Current address (with base offset) */
static inline uint32_t G_HERE(GlyphAsm *g) {
    return GLYPH_BASE + g->pos;
}

/* Define a label at current position */
static inline void G_LABEL(GlyphAsm *g, const char *name) {
    if (g->label_count < GLYPH_MAX_LABELS) {
        strncpy(g->labels[g->label_count].name, name, 31);
        g->labels[g->label_count].addr = G_HERE(g);
        g->label_count++;
    }
}

/* Find label address (returns 0 if not found) */
static inline uint32_t glyph_find_label(GlyphAsm *g, const char *name) {
    for (int i = 0; i < g->label_count; i++) {
        if (strcmp(g->labels[i].name, name) == 0)
            return g->labels[i].addr;
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Basic Instructions
 * ───────────────────────────────────────────────────────────────────────── */

/* :agX - Load literal byte into register */
static inline void G_LOAD_LIT(GlyphAsm *g, char reg, uint8_t val) {
    G_EMIT(g, ':'); G_EMIT(g, reg); G_EMIT(g, 'g'); G_EMIT(g, val);
}

/* :axN - Load hex digit (0-15) into register */
static inline void G_LOAD_HEX(GlyphAsm *g, char reg, uint8_t hex) {
    G_EMIT(g, ':'); G_EMIT(g, reg); G_EMIT(g, 'x');
    G_EMIT(g, (hex < 10) ? ('0' + hex) : ('a' + hex - 10));
}

/* :a.b - Copy register b to a */
static inline void G_COPY(GlyphAsm *g, char dst, char src) {
    G_EMIT(g, ':'); G_EMIT(g, dst); G_EMIT(g, '.'); G_EMIT(g, src);
}

/* Load 16-bit immediate into register (12 bytes) */
static inline void G_LOAD16(GlyphAsm *g, char reg, uint16_t val) {
    uint8_t hh = (val >> 12) & 0xF;
    uint8_t hl = (val >> 8) & 0xF;
    uint8_t lh = (val >> 4) & 0xF;
    uint8_t ll = val & 0xF;
    
    /* Use temp registers: reg for high byte build, '~' for low byte build */
    /* Build: ((hh << 4) | hl) << 8 | ((lh << 4) | ll) */
    
    /* High byte into reg */
    G_LOAD_HEX(g, reg, hh);           /* :Rxhh */
    G_EMIT(g, '<'); G_EMIT(g, reg); G_EMIT(g, reg); G_EMIT(g, '4'); /* <RR4 - shift left 4 */
    G_LOAD_HEX(g, '~', hl);           /* :~xhl */
    G_EMIT(g, '|'); G_EMIT(g, reg); G_EMIT(g, reg); G_EMIT(g, '~'); /* |RR~ - or with low nibble */
    
    /* Shift high byte left 8 */
    G_EMIT(g, '<'); G_EMIT(g, reg); G_EMIT(g, reg); G_EMIT(g, '8'); /* <RR8 */
    
    /* Low byte into temp */
    G_LOAD_HEX(g, '~', lh);           /* :~xlh */
    G_EMIT(g, '<'); G_EMIT(g, '~'); G_EMIT(g, '~'); G_EMIT(g, '4'); /* <~~4 */
    G_LOAD_HEX(g, '_', ll);           /* :_xll */
    G_EMIT(g, '|'); G_EMIT(g, '~'); G_EMIT(g, '~'); G_EMIT(g, '_'); /* |~~_ */
    
    /* Combine */
    G_EMIT(g, '|'); G_EMIT(g, reg); G_EMIT(g, reg); G_EMIT(g, '~'); /* |RR~ */
}

/* ─────────────────────────────────────────────────────────────────────────
 * Arithmetic
 * ───────────────────────────────────────────────────────────────────────── */

static inline void G_ADD(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '+'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_SUB(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '-'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_MUL(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '*'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_DIV(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '/'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_MOD(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '%'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Bitwise
 * ───────────────────────────────────────────────────────────────────────── */

static inline void G_AND(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '&'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_OR(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '|'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_XOR(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '^'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_NOT(GlyphAsm *g, char d, char s) {
    G_EMIT(g, '~'); G_EMIT(g, d); G_EMIT(g, s);
}

static inline void G_SHL(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '<'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

static inline void G_SHR(GlyphAsm *g, char d, char a, char b) {
    G_EMIT(g, '>'); G_EMIT(g, d); G_EMIT(g, a); G_EMIT(g, b);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Memory
 * ───────────────────────────────────────────────────────────────────────── */

/* @ab - Load from memory: a = mem[b] */
static inline void G_LOAD_MEM(GlyphAsm *g, char dst, char addr) {
    G_EMIT(g, '@'); G_EMIT(g, dst); G_EMIT(g, addr);
}

/* !ab - Store to memory: mem[a] = b */
static inline void G_STORE_MEM(GlyphAsm *g, char addr, char val) {
    G_EMIT(g, '!'); G_EMIT(g, addr); G_EMIT(g, val);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Ports (Resonance)
 * ───────────────────────────────────────────────────────────────────────── */

/* #<ab - Read from port: a = port[b] */
static inline void G_READ_PORT(GlyphAsm *g, char dst, char port) {
    G_EMIT(g, '#'); G_EMIT(g, '<'); G_EMIT(g, dst); G_EMIT(g, port);
}

/* #>ab - Write to port: port[a] = b */
static inline void G_WRITE_PORT(GlyphAsm *g, char port, char val) {
    G_EMIT(g, '#'); G_EMIT(g, '>'); G_EMIT(g, port); G_EMIT(g, val);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Control Flow
 * ───────────────────────────────────────────────────────────────────────── */

/* .a - Jump to address in register */
static inline void G_JUMP(GlyphAsm *g, char reg) {
    G_EMIT(g, '.'); G_EMIT(g, reg);
}

/* ;a - Call subroutine at address in register */
static inline void G_CALL(GlyphAsm *g, char reg) {
    G_EMIT(g, ';'); G_EMIT(g, reg);
}

/* , - Return from subroutine */
static inline void G_RET(GlyphAsm *g) {
    G_EMIT(g, ',');
}

/* ?=abt - If a == b, jump to address in t */
static inline void G_JEQ(GlyphAsm *g, char a, char b, char target) {
    G_EMIT(g, '?'); G_EMIT(g, '='); G_EMIT(g, a); G_EMIT(g, b); G_EMIT(g, target);
}

/* ?!abt - If a != b, jump to address in t */
static inline void G_JNE(GlyphAsm *g, char a, char b, char target) {
    G_EMIT(g, '?'); G_EMIT(g, '!'); G_EMIT(g, a); G_EMIT(g, b); G_EMIT(g, target);
}

/* ?>abt - If a > b, jump to address in t */
static inline void G_JGT(GlyphAsm *g, char a, char b, char target) {
    G_EMIT(g, '?'); G_EMIT(g, '>'); G_EMIT(g, a); G_EMIT(g, b); G_EMIT(g, target);
}

/* ?<abt - If a < b, jump to address in t */
static inline void G_JLT(GlyphAsm *g, char a, char b, char target) {
    G_EMIT(g, '?'); G_EMIT(g, '<'); G_EMIT(g, a); G_EMIT(g, b); G_EMIT(g, target);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Label References (for forward jumps)
 * ───────────────────────────────────────────────────────────────────────── */

/* Reserve space for a label reference (to be resolved later) */
static inline void G_LOAD16_LABEL(GlyphAsm *g, char reg, const char *label) {
    /* Check if label is already defined */
    uint32_t addr = glyph_find_label(g, label);
    if (addr) {
        G_LOAD16(g, reg, addr);
    } else {
        /* Record reference for later resolution */
        if (g->ref_count < GLYPH_MAX_REFS) {
            strncpy(g->refs[g->ref_count].name, label, 31);
            g->refs[g->ref_count].addr = g->pos;
            g->refs[g->ref_count].reg = reg;
            g->ref_count++;
        }
        /* Emit placeholder (will be patched) */
        G_LOAD16(g, reg, 0xFFFF);
    }
}

/* Resolve all label references */
static inline int glyph_resolve(GlyphAsm *g) {
    for (int i = 0; i < g->ref_count; i++) {
        uint32_t addr = glyph_find_label(g, g->refs[i].name);
        if (!addr) {
            fprintf(stderr, "glyphc: undefined label '%s'\n", g->refs[i].name);
            return -1;
        }
        
        /* Patch the G_LOAD16 instruction */
        uint32_t pos = g->refs[i].addr;
        char reg = g->refs[i].reg;
        
        uint8_t hh = (addr >> 12) & 0xF;
        uint8_t hl = (addr >> 8) & 0xF;
        uint8_t lh = (addr >> 4) & 0xF;
        uint8_t ll = addr & 0xF;
        
        /* Patch the hex digits in the instruction sequence */
        /* :Rxhh<RR4:~xhl|RR~<RR8:~xlh<~~4:_xll|~~_|RR~ */
        /* pos+0: : pos+1: R pos+2: x pos+3: hh */
        g->buf[pos + 3] = (hh < 10) ? ('0' + hh) : ('a' + hh - 10);
        /* pos+8: : pos+9: ~ pos+10: x pos+11: hl */
        g->buf[pos + 11] = (hl < 10) ? ('0' + hl) : ('a' + hl - 10);
        /* pos+20: : pos+21: ~ pos+22: x pos+23: lh */
        g->buf[pos + 23] = (lh < 10) ? ('0' + lh) : ('a' + lh - 10);
        /* pos+28: : pos+29: _ pos+30: x pos+31: ll */
        g->buf[pos + 31] = (ll < 10) ? ('0' + ll) : ('a' + ll - 10);
        
        /* Also patch the register names */
        g->buf[pos + 1] = reg;
        g->buf[pos + 5] = reg;
        g->buf[pos + 6] = reg;
        g->buf[pos + 13] = reg;
        g->buf[pos + 14] = reg;
        g->buf[pos + 17] = reg;
        g->buf[pos + 18] = reg;
        g->buf[pos + 37] = reg;
        g->buf[pos + 38] = reg;
    }
    return 0;
}

/* Write output to file */
static inline int glyph_write(GlyphAsm *g, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(g->buf, 1, g->pos, f);
    fclose(f);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Convenience: Common Console Operations
 * ───────────────────────────────────────────────────────────────────────── */

#define CON_WRITE 'o'   /* stdout port */
#define CON_READ  'c'   /* stdin port */
#define CON_ERROR 'e'   /* stderr port */

/* Print character in register to stdout */
static inline void G_PUTCHAR(GlyphAsm *g, char reg) {
    G_LOAD_LIT(g, '`', CON_WRITE);
    G_WRITE_PORT(g, '`', reg);
}

/* Read character from stdin into register */
static inline void G_GETCHAR(GlyphAsm *g, char reg) {
    G_LOAD_LIT(g, '`', CON_READ);
    G_READ_PORT(g, reg, '`');
}

/* Print immediate character */
static inline void G_PRINT_CHAR(GlyphAsm *g, char c) {
    G_LOAD_LIT(g, '`', c);
    G_LOAD_LIT(g, '[', CON_WRITE);
    G_WRITE_PORT(g, '[', '`');
}

#endif /* GLYPHC_H */
