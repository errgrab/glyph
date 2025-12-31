/*
 * glyph-dis - Glyph Disassembler
 * 
 * Disassembles Glyph bytecode into human-readable form.
 * 
 * Usage: glyph-dis <file.glyph>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BASE_ADDR 0x0100

static const char *data;
static size_t data_len;
static size_t pos;

static int peek(void) {
    return (pos < data_len) ? (unsigned char)data[pos] : -1;
}

static int next(void) {
    return (pos < data_len) ? (unsigned char)data[pos++] : -1;
}

static void print_char(int c) {
    if (c >= 32 && c < 127)
        printf("'%c'", c);
    else
        printf("0x%02X", c);
}

static void disasm_one(void) {
    unsigned int addr = BASE_ADDR + pos;
    int op = next();
    if (op < 0) return;
    
    printf("%04X: ", addr);
    
    int a, b, c, d;
    
    switch (op) {
    /* Arithmetic */
    case '+': case '-': case '*': case '/': case '%':
        a = next(); b = next(); c = next();
        printf("%c%c%c%c      ; %c = %c %c %c\n", 
               op, a, b, c, a, b, op, c);
        break;
    
    /* Bitwise (3 operands) */
    case '&': case '|': case '^': case '<': case '>':
        a = next(); b = next(); c = next();
        if (op == '<')
            printf("<%c%c%c      ; %c = %c << %c\n", a, b, c, a, b, c);
        else if (op == '>')
            printf(">%c%c%c      ; %c = %c >> %c\n", a, b, c, a, b, c);
        else
            printf("%c%c%c%c      ; %c = %c %c %c\n", op, a, b, c, a, b, op, c);
        break;
    
    /* Bitwise NOT (2 operands) */
    case '~':
        a = next(); b = next();
        printf("~%c%c       ; %c = ~%c\n", a, b, a, b);
        break;
    
    /* Load */
    case ':':
        a = next(); b = next();
        if (b == 'g') {
            c = next();
            printf(":%c%c", a, b);
            print_char(c);
            printf("    ; %c = ", a);
            print_char(c);
            printf(" (%d)\n", c);
        } else if (b == 'x') {
            c = next();
            int val = (c <= '9') ? c - '0' : (c | 32) - 'a' + 10;
            printf(":%c%c%c      ; %c = 0x%X (%d)\n", a, b, c, a, val, val);
        } else if (b == '.') {
            c = next();
            printf(":%c.%c      ; %c = %c\n", a, c, a, c);
        } else {
            printf(":%c%c       ; ??? (invalid load mode)\n", a, b);
        }
        break;
    
    /* Memory */
    case '@':
        a = next(); b = next();
        printf("@%c%c       ; %c = mem[%c]\n", a, b, a, b);
        break;
    case '!':
        a = next(); b = next();
        printf("!%c%c       ; mem[%c] = %c\n", a, b, a, b);
        break;
    
    /* Ports */
    case '#':
        a = next(); b = next(); c = next();
        if (a == '<') {
            printf("#<%c%c      ; %c = port[%c]", b, c, b, c);
            if (c == 'c' || (c >= '0' && c <= '9' && data[pos-1] == 'c'))
                printf(" (stdin)");
            printf("\n");
        } else if (a == '>') {
            printf("#>%c%c      ; port[%c] = %c", b, c, b, c);
            if (b == 'o') printf(" (stdout)");
            else if (b == 'e') printf(" (stderr)");
            else if (b == 'X') printf(" (exit)");
            printf("\n");
        } else {
            printf("#%c%c%c      ; ??? (invalid port op)\n", a, b, c);
        }
        break;
    
    /* Control flow */
    case '.':
        a = next();
        printf(".%c        ; jump to %c\n", a, a);
        break;
    
    case '?':
        a = next(); b = next(); c = next(); d = next();
        printf("?%c%c%c%c     ; if %c %c %c then jump to %c\n", 
               a, b, c, d, b, a, c, d);
        break;
    
    case ';':
        a = next();
        printf(";%c        ; call %c\n", a, a);
        break;
    
    case ',':
        printf(",         ; return\n");
        break;
    
    /* Whitespace */
    case ' ': case '\t': case '\n': case '\r': case '\f': case '\v':
        printf("          ; (whitespace)\n");
        break;
    
    /* Null terminator */
    case 0:
        printf("\\0        ; halt\n");
        break;
    
    /* Unknown */
    default:
        printf("%c         ; ??? (unknown opcode 0x%02X)\n", 
               (op >= 32 && op < 127) ? op : '?', op);
        break;
    }
}

static void usage(const char *prog) {
    fprintf(stderr, "glyph-dis: Glyph Disassembler\n\n");
    fprintf(stderr, "Usage: %s <file.glyph>\n", prog);
    fprintf(stderr, "       %s -e \"<code>\"\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }
    
    char *buf = NULL;
    
    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: -e requires code argument\n");
            return 1;
        }
        data = argv[2];
        data_len = strlen(argv[2]);
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        buf = malloc(size);
        if (!buf) {
            fprintf(stderr, "Error: out of memory\n");
            fclose(f);
            return 1;
        }
        
        data_len = fread(buf, 1, size, f);
        fclose(f);
        data = buf;
    }
    
    printf("; Glyph Disassembly - %zu bytes\n", data_len);
    printf("; Base address: 0x%04X\n\n", BASE_ADDR);
    
    pos = 0;
    while (pos < data_len) {
        disasm_one();
    }
    
    printf("\n; End at 0x%04X\n", (unsigned)(BASE_ADDR + pos));
    
    free(buf);
    return 0;
}
