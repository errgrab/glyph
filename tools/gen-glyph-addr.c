/*
 * gen-glyph-addr.c - Generate glyph-addr.glyph using glyphc
 * 
 * This generates a Glyph program that reads bytes from stdin
 * and prints their addresses in hex format.
 * 
 * Output format per byte: "XXXX  YY  'c'\n"
 * Where XXXX is the address, YY is the hex value, c is the character.
 */

#include "glyphc.h"

/* 
 * Register allocation:
 *   H, L  - Address high/low bytes (H:L is 16-bit address counter)
 *   b     - Current input byte
 *   n     - Temp for hex conversion
 *   p     - Char to print
 *   t     - Temp for division
 *   z     - Zero constant
 *   1     - One constant
 *   4     - Four constant (for shifts)
 *   8     - Eight constant (for shifts)
 *   A     - Ten constant (for hex conversion)
 *   F     - 0x0F mask
 *   D     - '0' (48) constant
 *   7     - 7 constant (for A-F offset)
 *   S     - Space character
 *   N     - Newline character
 *   Q     - Quote character
 *   w     - Console write port ('o')
 *   i     - Console read port ('c')
 *   M     - Loop address (for jumping back)
 *   E     - Exit address (for EOF)
 */

static uint8_t buffer[4096];

/* Emit hex digit print: print nibble in 'n' as hex char */
static void emit_print_hex_nibble(GlyphAsm *g) {
    /* Branchless: char = n + '0' + (n/10)*7
     * If n < 10: n + 48 + 0 = digit
     * If n >= 10: n + 48 + 7 = letter
     */
    G_ADD(g, 'p', 'n', 'D');      /* p = n + '0' */
    G_DIV(g, 't', 'n', 'A');      /* t = n / 10 (0 or 1) */
    G_MUL(g, 't', 't', '7');      /* t = t * 7 */
    G_ADD(g, 'p', 'p', 't');      /* p = p + t */
    G_WRITE_PORT(g, 'w', 'p');    /* print p */
}

/* Emit code to print byte in register as 2 hex digits */
static void emit_print_hex_byte(GlyphAsm *g, char reg) {
    /* High nibble */
    G_COPY(g, 'n', reg);
    G_SHR(g, 'n', 'n', '4');      /* n = reg >> 4 */
    G_AND(g, 'n', 'n', 'F');      /* n = n & 0x0F */
    emit_print_hex_nibble(g);
    
    /* Low nibble */
    G_COPY(g, 'n', reg);
    G_AND(g, 'n', 'n', 'F');      /* n = reg & 0x0F */
    emit_print_hex_nibble(g);
}

int main(void) {
    GlyphAsm g;
    glyph_init_asm(&g, buffer, sizeof(buffer));
    
    /* ─────────────────────────────────────────────────────────────────────
     * Initialization
     * ───────────────────────────────────────────────────────────────────── */
    
    /* Console ports */
    G_LOAD_LIT(&g, 'w', 'o');     /* w = stdout port */
    G_LOAD_LIT(&g, 'i', 'c');     /* i = stdin port */
    
    /* Address counter starts at 0x0100 */
    G_LOAD_HEX(&g, 'H', 1);       /* H = 1 (high byte) */
    G_LOAD_HEX(&g, 'L', 0);       /* L = 0 (low byte) */
    
    /* Constants */
    G_LOAD_HEX(&g, 'z', 0);       /* z = 0 */
    G_LOAD_HEX(&g, '1', 1);       /* 1 = 1 */
    G_LOAD_HEX(&g, '4', 4);       /* 4 = 4 */
    G_LOAD_HEX(&g, '8', 8);       /* 8 = 8 */
    G_LOAD_HEX(&g, 'A', 0xA);     /* A = 10 */
    G_LOAD_HEX(&g, 'F', 0xF);     /* F = 15 (mask) */
    G_LOAD_LIT(&g, 'D', '0');     /* D = '0' (48) */
    G_LOAD_HEX(&g, '7', 7);       /* 7 = 7 */
    G_LOAD_LIT(&g, 'S', ' ');     /* S = space */
    G_LOAD_LIT(&g, 'N', '\n');    /* N = newline */
    G_LOAD_LIT(&g, 'Q', '\'');    /* Q = quote */
    
    /* Forward references for jump targets */
    G_LOAD16_LABEL(&g, 'M', "loop");
    G_LOAD16_LABEL(&g, 'E', "exit");
    
    /* ─────────────────────────────────────────────────────────────────────
     * Main Loop
     * ───────────────────────────────────────────────────────────────────── */
    
    G_LABEL(&g, "loop");
    
    /* Read byte from stdin */
    G_READ_PORT(&g, 'b', 'i');
    
    /* If b == 0 (EOF), exit */
    G_JEQ(&g, 'b', 'z', 'E');
    
    /* Print address high byte (H) */
    emit_print_hex_byte(&g, 'H');
    
    /* Print address low byte (L) */
    emit_print_hex_byte(&g, 'L');
    
    /* Print "  " (two spaces) */
    G_WRITE_PORT(&g, 'w', 'S');
    G_WRITE_PORT(&g, 'w', 'S');
    
    /* Print byte value in hex */
    emit_print_hex_byte(&g, 'b');
    
    /* Print "  '" */
    G_WRITE_PORT(&g, 'w', 'S');
    G_WRITE_PORT(&g, 'w', 'S');
    G_WRITE_PORT(&g, 'w', 'Q');
    
    /* Print the actual character (or '.' for non-printable) */
    /* For simplicity, just print the byte */
    G_WRITE_PORT(&g, 'w', 'b');
    
    /* Print "'\n" */
    G_WRITE_PORT(&g, 'w', 'Q');
    G_WRITE_PORT(&g, 'w', 'N');
    
    /* Increment 16-bit address (L++, if L==0 then H++) */
    G_ADD(&g, 'L', 'L', '1');
    G_JNE(&g, 'L', 'z', 'M');     /* If L != 0, loop */
    G_ADD(&g, 'H', 'H', '1');     /* L wrapped, increment H */
    G_JUMP(&g, 'M');              /* Loop */
    
    /* ─────────────────────────────────────────────────────────────────────
     * Exit
     * ───────────────────────────────────────────────────────────────────── */
    
    G_LABEL(&g, "exit");
    /* Program ends naturally (halt on unknown opcode or null) */
    G_EMIT(&g, 0);
    
    /* ─────────────────────────────────────────────────────────────────────
     * Resolve labels and write output
     * ───────────────────────────────────────────────────────────────────── */
    
    if (glyph_resolve(&g) < 0) {
        fprintf(stderr, "Error resolving labels\n");
        return 1;
    }
    
    /* Print stats */
    printf("; Generated glyph-addr.glyph\n");
    printf("; Size: %u bytes\n", g.pos);
    printf("; Labels:\n");
    for (int i = 0; i < g.label_count; i++) {
        printf(";   %s = 0x%04X\n", g.labels[i].name, g.labels[i].addr);
    }
    
    if (glyph_write(&g, "examples/glyph-addr.glyph") < 0) {
        fprintf(stderr, "Error writing output\n");
        return 1;
    }
    
    printf("; Written to examples/glyph-addr.glyph\n");
    return 0;
}
