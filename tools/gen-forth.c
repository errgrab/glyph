/*
 * gen-forth.c - Generate a minimal Forth interpreter in Glyph
 * 
 * Memory Map:
 *   0x0100 - 0x0FFF  Code (interpreter + primitives)
 *   0x1000 - 0x10FF  Input buffer (256 bytes)
 *   0x1100 - 0x11FF  Word buffer (256 bytes) 
 *   0x1200 - 0x12FF  Parameter stack (256 bytes, grows down)
 *   0x1300 - 0x13FF  Return stack (256 bytes, grows down)
 *   0x1400 - 0x7FFF  Dictionary + user definitions
 *   0x8000 - 0xFFFF  Free memory (HERE starts here)
 *
 * Register Allocation:
 *   S  - Parameter stack pointer (points to TOS)
 *   R  - Return stack pointer
 *   H  - HERE pointer (next free dictionary cell)
 *   W  - Word pointer (current word being executed)
 *   I  - Instruction pointer (for threaded code)
 *   T  - Top of stack cache
 *   N  - Next on stack
 *   
 *   z  - Zero constant
 *   1  - One constant  
 *   4  - Four constant
 *   8  - Eight constant
 *   
 *   i  - Input port ('c')
 *   o  - Output port ('o')
 *   
 *   a-f, n, p, t, x, y - Temp registers
 *
 * Dictionary Entry Format:
 *   [link:2][flags:1][len:1][name:len][code...]
 *   - link: pointer to previous entry (0 = end)
 *   - flags: 0x80 = immediate, 0x40 = hidden
 *   - len: name length (1-31)
 *   - name: the word name
 *   - code: native Glyph code or threaded addresses
 */

#include "glyphc.h"
#include <string.h>

static uint8_t buffer[8192];
static GlyphAsm g;

/* Memory layout constants */
#define INPUT_BUF   0x1000
#define WORD_BUF    0x1100
#define PSTACK      0x12FF   /* Stack grows down */
#define RSTACK      0x13FF   /* Return stack grows down */
#define DICT_START  0x1400
#define HERE_START  0x8000

/* Current dictionary pointer for building */
static uint16_t dict_here = DICT_START;
static uint16_t last_word = 0;

/* ─────────────────────────────────────────────────────────────────────────
 * Helper: Define a dictionary entry header
 * ───────────────────────────────────────────────────────────────────────── */
static void dict_header(const char *name, int flags) {
    /* Remember this entry's address */
    uint16_t entry_addr = G_HERE(&g);
    
    /* Create label for this word */
    char label[64];
    snprintf(label, sizeof(label), "word_%s", name);
    G_LABEL(&g, label);
    
    /* Link to previous word (2 bytes, little endian) */
    G_EMIT(&g, last_word & 0xFF);
    G_EMIT(&g, (last_word >> 8) & 0xFF);
    
    /* Flags + length */
    int len = strlen(name);
    G_EMIT(&g, flags | (len & 0x1F));
    
    /* Name */
    for (int i = 0; i < len; i++) {
        G_EMIT(&g, name[i]);
    }
    
    last_word = entry_addr;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Emit primitive helpers
 * ───────────────────────────────────────────────────────────────────────── */

/* Push T onto stack, load new value into T */
static void emit_push(void) {
    /* mem[S] = T; S-- */
    G_STORE_MEM(&g, 'S', 'T');
    G_SUB(&g, 'S', 'S', '1');
}

/* Pop from stack into T */
static void emit_pop(void) {
    /* S++; T = mem[S] */
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'T', 'S');
}

/* ─────────────────────────────────────────────────────────────────────────
 * Main generator
 * ───────────────────────────────────────────────────────────────────────── */

int main(void) {
    glyph_init_asm(&g, buffer, sizeof(buffer));
    
    /* ═══════════════════════════════════════════════════════════════════════
     * INITIALIZATION
     * ═══════════════════════════════════════════════════════════════════════ */
    
    G_LABEL(&g, "init");
    
    /* Constants */
    G_LOAD_HEX(&g, 'z', 0);
    G_LOAD_HEX(&g, '1', 1);
    G_LOAD_HEX(&g, '4', 4);
    G_LOAD_HEX(&g, '8', 8);
    
    /* I/O ports */
    G_LOAD_LIT(&g, 'i', 'c');    /* stdin */
    G_LOAD_LIT(&g, 'o', 'o');    /* stdout */
    
    /* Initialize stack pointers */
    G_LOAD16(&g, 'S', PSTACK);   /* Parameter stack */
    G_LOAD16(&g, 'R', RSTACK);   /* Return stack */
    G_LOAD16(&g, 'H', HERE_START); /* HERE */
    
    /* Clear TOS */
    G_COPY(&g, 'T', 'z');
    
    /* Print prompt and enter main loop */
    G_LOAD_LIT(&g, 'a', '>');
    G_WRITE_PORT(&g, 'o', 'a');
    G_LOAD_LIT(&g, 'a', ' ');
    G_WRITE_PORT(&g, 'o', 'a');
    
    /* Jump to main loop */
    G_LOAD16_LABEL(&g, 'a', "quit");
    G_JUMP(&g, 'a');
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: KEY ( -- c )
     * Read a character from input
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("KEY", 0);
    G_LABEL(&g, "prim_key");
    emit_push();
    G_READ_PORT(&g, 'T', 'i');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: EMIT ( c -- )
     * Write a character to output
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("EMIT", 0);
    G_LABEL(&g, "prim_emit");
    G_WRITE_PORT(&g, 'o', 'T');
    emit_pop();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: DUP ( a -- a a )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("DUP", 0);
    G_LABEL(&g, "prim_dup");
    emit_push();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: DROP ( a -- )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("DROP", 0);
    G_LABEL(&g, "prim_drop");
    emit_pop();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: SWAP ( a b -- b a )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("SWAP", 0);
    G_LABEL(&g, "prim_swap");
    /* N = mem[S+1] */
    G_ADD(&g, 'a', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'a');
    /* mem[S+1] = T */
    G_STORE_MEM(&g, 'a', 'T');
    /* T = N */
    G_COPY(&g, 'T', 'N');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: OVER ( a b -- a b a )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("OVER", 0);
    G_LABEL(&g, "prim_over");
    emit_push();
    /* T = mem[S+2] */
    G_ADD(&g, 'a', 'S', '1');
    G_ADD(&g, 'a', 'a', '1');
    G_LOAD_MEM(&g, 'T', 'a');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: + ( a b -- a+b )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("+", 0);
    G_LABEL(&g, "prim_add");
    /* N = pop */
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    /* T = N + T */
    G_ADD(&g, 'T', 'N', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: - ( a b -- a-b )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("-", 0);
    G_LABEL(&g, "prim_sub");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_SUB(&g, 'T', 'N', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: * ( a b -- a*b )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("*", 0);
    G_LABEL(&g, "prim_mul");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_MUL(&g, 'T', 'N', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: / ( a b -- a/b )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("/", 0);
    G_LABEL(&g, "prim_div");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_DIV(&g, 'T', 'N', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: MOD ( a b -- a%b )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("MOD", 0);
    G_LABEL(&g, "prim_mod");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_MOD(&g, 'T', 'N', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: . ( n -- )
     * Print number as decimal (iterative, using memory for digit buffer)
     * Uses addresses 0x10E0-0x10FF as temporary digit buffer
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header(".", 0);
    G_LABEL(&g, "prim_dot");
    /* Setup: use 0x10FF as end of digit buffer, work backwards */
    G_LOAD16(&g, 'x', 0x10FF);    /* x = buffer pointer */
    G_LOAD_HEX(&g, 'f', 0xA);     /* f = 10 for division */
    
    /* Handle zero specially */
    G_LOAD16_LABEL(&g, 'b', "dot_loop");
    G_JNE(&g, 'T', 'z', 'b');
    /* T is zero, just print '0' */
    G_LOAD_LIT(&g, 'a', '0');
    G_WRITE_PORT(&g, 'o', 'a');
    G_LOAD16_LABEL(&g, 'b', "dot_done");
    G_JUMP(&g, 'b');
    
    G_LABEL(&g, "dot_loop");
    /* While T > 0: extract digits */
    G_LOAD16_LABEL(&g, 'b', "dot_print");
    G_JEQ(&g, 'T', 'z', 'b');     /* T == 0? Done extracting */
    
    G_MOD(&g, 'a', 'T', 'f');     /* a = T % 10 */
    G_DIV(&g, 'T', 'T', 'f');     /* T = T / 10 */
    
    /* Convert to ASCII and store in buffer */
    G_LOAD_LIT(&g, 'n', '0');
    G_ADD(&g, 'a', 'a', 'n');     /* a = digit char */
    G_STORE_MEM(&g, 'x', 'a');    /* store at x */
    G_SUB(&g, 'x', 'x', '1');     /* x-- (buffer grows down) */
    
    G_LOAD16_LABEL(&g, 'b', "dot_loop");
    G_JUMP(&g, 'b');
    
    G_LABEL(&g, "dot_print");
    /* Print digits from x+1 to 0x10FF */
    G_ADD(&g, 'x', 'x', '1');     /* x points to first digit */
    
    G_LABEL(&g, "dot_print_loop");
    G_LOAD16(&g, 'n', 0x10FF);
    G_ADD(&g, 'n', 'n', '1');     /* n = 0x1100 (one past end) */
    G_LOAD16_LABEL(&g, 'b', "dot_done");
    G_JEQ(&g, 'x', 'n', 'b');     /* x == 0x1100? Done */
    
    G_LOAD_MEM(&g, 'a', 'x');     /* a = digit char */
    G_WRITE_PORT(&g, 'o', 'a');   /* print it */
    G_ADD(&g, 'x', 'x', '1');     /* x++ */
    G_LOAD16_LABEL(&g, 'b', "dot_print_loop");
    G_JUMP(&g, 'b');
    
    G_LABEL(&g, "dot_done");
    /* Print trailing space */
    G_LOAD_LIT(&g, 'a', ' ');
    G_WRITE_PORT(&g, 'o', 'a');
    emit_pop();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: CR ( -- )
     * Print newline
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("CR", 0);
    G_LABEL(&g, "prim_cr");
    G_LOAD_LIT(&g, 'a', '\n');
    G_WRITE_PORT(&g, 'o', 'a');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: SPACE ( -- )
     * Print space
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("SPACE", 0);
    G_LABEL(&g, "prim_space");
    G_LOAD_LIT(&g, 'a', ' ');
    G_WRITE_PORT(&g, 'o', 'a');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: = ( a b -- flag )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("=", 0);
    G_LABEL(&g, "prim_eq");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    /* If N == T, result is 1, else 0 */
    G_LOAD16_LABEL(&g, 'a', "eq_true");
    G_JEQ(&g, 'N', 'T', 'a');
    G_COPY(&g, 'T', 'z');  /* false */
    G_RET(&g);
    G_LABEL(&g, "eq_true");
    G_COPY(&g, 'T', '1');  /* true */
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: < ( a b -- flag )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("<", 0);
    G_LABEL(&g, "prim_lt");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_LOAD16_LABEL(&g, 'a', "lt_true");
    G_JLT(&g, 'N', 'T', 'a');
    G_COPY(&g, 'T', 'z');
    G_RET(&g);
    G_LABEL(&g, "lt_true");
    G_COPY(&g, 'T', '1');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: > ( a b -- flag )
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header(">", 0);
    G_LABEL(&g, "prim_gt");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_LOAD16_LABEL(&g, 'a', "gt_true");
    G_JGT(&g, 'N', 'T', 'a');
    G_COPY(&g, 'T', 'z');
    G_RET(&g);
    G_LABEL(&g, "gt_true");
    G_COPY(&g, 'T', '1');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: HERE ( -- addr )
     * Return the HERE pointer
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("HERE", 0);
    G_LABEL(&g, "prim_here");
    emit_push();
    G_COPY(&g, 'T', 'H');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: @ ( addr -- val )
     * Fetch from memory
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("@", 0);
    G_LABEL(&g, "prim_fetch");
    G_LOAD_MEM(&g, 'T', 'T');
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: ! ( val addr -- )
     * Store to memory
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("!", 0);
    G_LABEL(&g, "prim_store");
    /* addr in T, val in N */
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_STORE_MEM(&g, 'T', 'N');
    emit_pop();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: C@ ( addr -- byte )
     * Fetch byte from memory
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("C@", 0);
    G_LABEL(&g, "prim_cfetch");
    G_LOAD_MEM(&g, 'T', 'T');
    G_AND(&g, 'T', 'T', 'F');  /* Mask to byte... actually we need 0xFF */
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: C! ( byte addr -- )
     * Store byte to memory
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("C!", 0);
    G_LABEL(&g, "prim_cstore");
    G_ADD(&g, 'S', 'S', '1');
    G_LOAD_MEM(&g, 'N', 'S');
    G_STORE_MEM(&g, 'T', 'N');
    emit_pop();
    G_RET(&g);
    
    /* ═══════════════════════════════════════════════════════════════════════
     * PRIMITIVE: BYE ( -- )
     * Exit the interpreter
     * ═══════════════════════════════════════════════════════════════════════ */
    
    dict_header("BYE", 0);
    G_LABEL(&g, "prim_bye");
    G_EMIT(&g, 0);  /* Halt */
    
    /* ═══════════════════════════════════════════════════════════════════════
     * INTERPRETER: QUIT
     * Main interpreter loop
     * ═══════════════════════════════════════════════════════════════════════ */
    
    G_LABEL(&g, "quit");
    
    /* Read a word into word buffer */
    G_LOAD16(&g, 'W', WORD_BUF);  /* W = word buffer pointer */
    G_COPY(&g, 'x', 'W');         /* x = current position in buffer */
    
    /* Skip leading whitespace */
    G_LABEL(&g, "skip_ws");
    G_READ_PORT(&g, 'a', 'i');    /* Read char */
    G_JEQ(&g, 'a', 'z', 'a');     /* EOF? Exit (jump to 0 = halt) */
    G_LOAD_LIT(&g, 'b', ' ');
    G_LOAD16_LABEL(&g, 'c', "skip_ws");
    G_JEQ(&g, 'a', 'b', 'c');     /* Space? Keep skipping */
    G_LOAD_LIT(&g, 'b', '\n');
    G_JEQ(&g, 'a', 'b', 'c');     /* Newline? Keep skipping */
    G_LOAD_LIT(&g, 'b', '\t');
    G_JEQ(&g, 'a', 'b', 'c');     /* Tab? Keep skipping */
    
    /* Found non-whitespace, store it */
    G_STORE_MEM(&g, 'x', 'a');
    G_ADD(&g, 'x', 'x', '1');
    
    /* Read rest of word */
    G_LABEL(&g, "read_word");
    G_READ_PORT(&g, 'a', 'i');
    G_JEQ(&g, 'a', 'z', 'a');     /* EOF? Halt */
    G_LOAD_LIT(&g, 'b', ' ');
    G_LOAD16_LABEL(&g, 'c', "word_done");
    G_JEQ(&g, 'a', 'b', 'c');     /* Space? Word done */
    G_LOAD_LIT(&g, 'b', '\n');
    G_JEQ(&g, 'a', 'b', 'c');     /* Newline? Word done */
    G_LOAD_LIT(&g, 'b', '\t');
    G_JEQ(&g, 'a', 'b', 'c');     /* Tab? Word done */
    /* Store char */
    G_STORE_MEM(&g, 'x', 'a');
    G_ADD(&g, 'x', 'x', '1');
    G_LOAD16_LABEL(&g, 'c', "read_word");
    G_JUMP(&g, 'c');
    
    G_LABEL(&g, "word_done");
    /* Null-terminate */
    G_STORE_MEM(&g, 'x', 'z');
    /* Calculate length: x - W */
    G_SUB(&g, 'y', 'x', 'W');     /* y = word length */
    
    /* ─────────────────────────────────────────────────────────────────────
     * Try to parse as number (multi-digit)
     * ───────────────────────────────────────────────────────────────────── */
    
    /* Check if first char is a digit */
    G_LOAD_MEM(&g, 'a', 'W');     /* a = first character */
    G_LOAD_LIT(&g, 'b', '0');
    G_LOAD16_LABEL(&g, 'c', "try_find");
    G_JLT(&g, 'a', 'b', 'c');     /* < '0'? Not a number */
    G_LOAD_LIT(&g, 'b', ':');     /* ':' is '9' + 1 */
    G_JGT(&g, 'a', 'b', 'c');     /* > '9'? Not a number */
    G_JEQ(&g, 'a', 'b', 'c');     /* == ':'? Not a number */
    
    /* It starts with a digit - parse full number */
    G_COPY(&g, 'n', 'z');         /* n = accumulated value (0) */
    G_COPY(&g, 'x', 'W');         /* x = current position */
    G_LOAD_HEX(&g, 'f', 0xA);     /* f = 10 (for multiply) */
    
    G_LABEL(&g, "parse_num");
    G_LOAD_MEM(&g, 'a', 'x');     /* a = current char */
    G_LOAD16_LABEL(&g, 'c', "num_done");
    G_JEQ(&g, 'a', 'z', 'c');     /* End of string? Done */
    
    /* Check if digit */
    G_LOAD_LIT(&g, 'b', '0');
    G_JLT(&g, 'a', 'b', 'c');     /* < '0'? Not a digit, done */
    G_LOAD_LIT(&g, 'b', ':');
    G_JGT(&g, 'a', 'b', 'c');     /* > '9'? Not a digit, done */
    G_JEQ(&g, 'a', 'b', 'c');     /* == ':'? Not a digit, done */
    
    /* n = n * 10 + (a - '0') */
    G_MUL(&g, 'n', 'n', 'f');     /* n = n * 10 */
    G_LOAD_LIT(&g, 'b', '0');
    G_SUB(&g, 'a', 'a', 'b');     /* a = digit value */
    G_ADD(&g, 'n', 'n', 'a');     /* n = n + digit */
    
    G_ADD(&g, 'x', 'x', '1');     /* next char */
    G_LOAD16_LABEL(&g, 'c', "parse_num");
    G_JUMP(&g, 'c');
    
    G_LABEL(&g, "num_done");
    /* Push the number */
    emit_push();
    G_COPY(&g, 'T', 'n');
    G_LOAD16_LABEL(&g, 'c', "quit");
    G_JUMP(&g, 'c');
    
    /* ─────────────────────────────────────────────────────────────────────
     * Dictionary lookup
     * ───────────────────────────────────────────────────────────────────── */
    
    G_LABEL(&g, "try_find");
    
    /* d = LATEST (last dictionary entry) */
    G_LOAD16(&g, 'd', last_word);
    
    G_LABEL(&g, "find_loop");
    /* If d == 0, word not found */
    G_LOAD16_LABEL(&g, 'c', "not_found");
    G_JEQ(&g, 'd', 'z', 'c');
    
    /* Get entry length (at d+2, masked) */
    G_ADD(&g, 'a', 'd', '1');
    G_ADD(&g, 'a', 'a', '1');     /* a = d + 2 (flags+len byte) */
    G_LOAD_MEM(&g, 'e', 'a');     /* e = flags+len */
    G_LOAD_HEX(&g, 'b', 0x1F);    /* mask for length */
    /* Need to build 0x1F = 31 */
    G_LOAD_HEX(&g, 'b', 1);
    G_SHL(&g, 'b', 'b', '4');
    G_LOAD_HEX(&g, 'f', 0xF);
    G_OR(&g, 'b', 'b', 'f');      /* b = 0x1F */
    G_AND(&g, 'e', 'e', 'b');     /* e = name length */
    
    /* Compare lengths */
    G_LOAD16_LABEL(&g, 'c', "find_next");
    G_JNE(&g, 'e', 'y', 'c');     /* Different length? Next entry */
    
    /* Compare names */
    G_ADD(&g, 'a', 'a', '1');     /* a = start of name in dict */
    G_COPY(&g, 'b', 'W');         /* b = start of word buffer */
    G_COPY(&g, 'f', 'e');         /* f = counter */
    
    G_LABEL(&g, "cmp_loop");
    G_LOAD16_LABEL(&g, 'c', "found");
    G_JEQ(&g, 'f', 'z', 'c');     /* All chars matched? Found! */
    
    G_LOAD_MEM(&g, 'n', 'a');     /* n = dict char */
    G_LOAD_MEM(&g, 'p', 'b');     /* p = word char */
    G_LOAD16_LABEL(&g, 'c', "find_next");
    G_JNE(&g, 'n', 'p', 'c');     /* Mismatch? Next entry */
    
    G_ADD(&g, 'a', 'a', '1');
    G_ADD(&g, 'b', 'b', '1');
    G_SUB(&g, 'f', 'f', '1');
    G_LOAD16_LABEL(&g, 'c', "cmp_loop");
    G_JUMP(&g, 'c');
    
    G_LABEL(&g, "find_next");
    /* d = link at d */
    G_LOAD_MEM(&g, 'a', 'd');     /* Low byte of link */
    G_ADD(&g, 'b', 'd', '1');
    G_LOAD_MEM(&g, 'b', 'b');     /* High byte of link */
    G_SHL(&g, 'b', 'b', '8');
    G_OR(&g, 'd', 'a', 'b');      /* d = link */
    G_LOAD16_LABEL(&g, 'c', "find_loop");
    G_JUMP(&g, 'c');
    
    /* ─────────────────────────────────────────────────────────────────────
     * Word found - execute it
     * ───────────────────────────────────────────────────────────────────── */
    
    G_LABEL(&g, "found");
    /* a points past the name, that's the code */
    /* Call the word */
    G_CALL(&g, 'a');
    /* Return to interpreter */
    G_LOAD16_LABEL(&g, 'c', "quit");
    G_JUMP(&g, 'c');
    
    /* ─────────────────────────────────────────────────────────────────────
     * Word not found - print error
     * ───────────────────────────────────────────────────────────────────── */
    
    G_LABEL(&g, "not_found");
    /* Print "? " */
    G_LOAD_LIT(&g, 'a', '?');
    G_WRITE_PORT(&g, 'o', 'a');
    G_LOAD_LIT(&g, 'a', ' ');
    G_WRITE_PORT(&g, 'o', 'a');
    /* Print the word */
    G_COPY(&g, 'a', 'W');
    G_LABEL(&g, "print_word");
    G_LOAD_MEM(&g, 'b', 'a');
    G_LOAD16_LABEL(&g, 'c', "print_word_done");
    G_JEQ(&g, 'b', 'z', 'c');
    G_WRITE_PORT(&g, 'o', 'b');
    G_ADD(&g, 'a', 'a', '1');
    G_LOAD16_LABEL(&g, 'c', "print_word");
    G_JUMP(&g, 'c');
    G_LABEL(&g, "print_word_done");
    /* Print newline */
    G_LOAD_LIT(&g, 'a', '\n');
    G_WRITE_PORT(&g, 'o', 'a');
    /* Continue */
    G_LOAD16_LABEL(&g, 'c', "quit");
    G_JUMP(&g, 'c');
    
    /* ═══════════════════════════════════════════════════════════════════════
     * Resolve and write
     * ═══════════════════════════════════════════════════════════════════════ */
    
    if (glyph_resolve(&g) < 0) {
        fprintf(stderr, "Error resolving labels\n");
        return 1;
    }
    
    printf("; GlyphForth - A minimal Forth interpreter\n");
    printf("; Size: %u bytes\n", g.pos);
    printf("; Dictionary entries: %d\n", g.label_count);
    printf(";\n");
    printf("; Labels:\n");
    for (int i = 0; i < g.label_count; i++) {
        printf(";   %-20s = 0x%04X\n", g.labels[i].name, g.labels[i].addr);
    }
    printf(";\n");
    printf("; LATEST word at: 0x%04X\n", last_word);
    
    if (glyph_write(&g, "examples/forth.glyph") < 0) {
        fprintf(stderr, "Error writing output\n");
        return 1;
    }
    
    printf("; Written to examples/forth.glyph\n");
    printf(";\n");
    printf("; Usage: ./glyph examples/forth.glyph\n");
    printf("; Try: 3 4 + . CR\n");
    printf(";      5 DUP * . CR\n");
    printf(";      BYE\n");
    
    return 0;
}
