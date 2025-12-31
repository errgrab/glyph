/*
 * glyph-addr: Display byte addresses in Glyph source files
 * 
 * Shows the memory address of each byte when loaded at 0x0100.
 * Useful for calculating jump targets.
 *
 * Usage: glyph-addr <file.glyph>
 *        glyph-addr -e "<code>"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BASE_ADDR 0x0100

static void print_header(void) {
    printf("ADDR  HEX  CHR  | ADDR  HEX  CHR  | ADDR  HEX  CHR  | ADDR  HEX  CHR\n");
    printf("----------------------------------------------------------------------\n");
}

static void process(const char *data, size_t len) {
    unsigned int addr = BASE_ADDR;
    int col = 0;
    
    print_header();
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = data[i];
        
        /* Show printable char or dot for non-printable */
        char display = (c >= 32 && c < 127) ? c : '.';
        
        printf("%04X  %02X   '%c'", addr, c, display);
        
        col++;
        if (col < 4) {
            printf("  | ");
        } else {
            printf("\n");
            col = 0;
        }
        
        addr++;
    }
    
    if (col > 0) printf("\n");
    
    printf("----------------------------------------------------------------------\n");
    printf("Total: %zu bytes (0x%04X - 0x%04X)\n", len, BASE_ADDR, addr - 1);
}

static void usage(const char *prog) {
    fprintf(stderr, "glyph-addr: Display byte addresses in Glyph source\n\n");
    fprintf(stderr, "Usage: %s <file.glyph>\n", prog);
    fprintf(stderr, "       %s -e \"<code>\"\n\n", prog);
    fprintf(stderr, "Addresses start at 0x0100 (Glyph program base).\n");
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
    
    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: -e requires code argument\n");
            return 1;
        }
        process(argv[2], strlen(argv[2]));
        return 0;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fprintf(stderr, "Error: empty file '%s'\n", argv[1]);
        fclose(f);
        return 1;
    }
    
    char *data = malloc(size);
    if (!data) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(f);
        return 1;
    }
    
    size_t nread = fread(data, 1, size, f);
    fclose(f);
    
    process(data, nread);
    
    free(data);
    return 0;
}
