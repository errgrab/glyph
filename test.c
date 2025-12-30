/* Glyph VM tests */
#define GLYPH_IMPL
#include "glyph.h"
#include <stdio.h>

#define TEST(name) static void test_##name(void)
#define RUN(name) printf("%-20s", #name); test_##name(); printf("OK\n")
#define ASSERT(x) do { if(!(x)) { printf("FAIL: %s\n", #x); return; } } while(0)

static Glyph vm;
static uint8_t mem[256];

static void run(const char *prog) {
    glyph_init(&vm, mem, sizeof(mem));
    memcpy(mem, prog, strlen(prog) + 1);
    glyph_run(&vm);
}

TEST(arithmetic) {
    run(":ad5 :bd3 +cab -dab *eab /fab");
    ASSERT(vm.reg['c'] == 8);
    ASSERT(vm.reg['d'] == 2);
    ASSERT(vm.reg['e'] == 15);
    ASSERT(vm.reg['f'] == 1);
}

TEST(bitwise) {
    run(":axF :bx7 &cab |dab ^eab ~fa");
    ASSERT(vm.reg['c'] == 7);
    ASSERT(vm.reg['d'] == 15);
    ASSERT(vm.reg['e'] == 8);
    ASSERT(vm.reg['f'] == ~15u);
}

TEST(shifts) {
    run(":ad4 :bd2 <cab >dab");
    ASSERT(vm.reg['c'] == 16);
    ASSERT(vm.reg['d'] == 1);
}

TEST(memory) {
    uint8_t p[] = {':','a','g',50, ':','b','g',42, '!','a','b', '@','c','a', 0};
    glyph_init(&vm, mem, sizeof(mem));
    memcpy(mem, p, sizeof(p));
    glyph_run(&vm);
    ASSERT(vm.reg['c'] == 42);
    ASSERT(mem[50] == 42);
}

TEST(ports) {
    uint8_t p1[] = {':','a','g',5, ':','b','g',99, ')','a','b', 0};
    glyph_init(&vm, mem, sizeof(mem));
    memcpy(mem, p1, sizeof(p1));
    glyph_run(&vm);
    ASSERT(vm.port[5] == 99);
    
    uint8_t p2[] = {':','a','g',10, '(','b','a', 0};
    glyph_init(&vm, mem, sizeof(mem));
    vm.port[10] = 77;
    memcpy(mem, p2, sizeof(p2));
    glyph_run(&vm);
    ASSERT(vm.reg['b'] == 77);
}

TEST(jump) {
    uint8_t p[] = {':','j','g',12, '.','j', ':','a','g',1, 0};
    glyph_init(&vm, mem, sizeof(mem));
    memcpy(mem, p, sizeof(p));
    glyph_run(&vm);
    ASSERT(vm.reg['a'] == 0);  /* skipped */
}

TEST(conditional) {
    run(":ad5 :bd5 ?=ab :cd1 :dd2");
    ASSERT(vm.reg['c'] == 1 && vm.reg['d'] == 2);

    run(":ad5 :bd3 ?=ab :cd1");
    ASSERT(vm.reg['c'] == 0);  /* skipped due to inequality */
}

TEST(copy) {
    uint8_t p[] = {':','a','g',42, ':','b','.','a', 0};
    glyph_init(&vm, mem, sizeof(mem));
    memcpy(mem, p, sizeof(p));
    glyph_run(&vm);
    ASSERT(vm.reg['b'] == 42);
}

int main(void) {
    printf("Glyph VM Tests\n==============\n");
    RUN(arithmetic);
    RUN(bitwise);
    RUN(shifts);
    RUN(memory);
    RUN(ports);
    RUN(jump);
    RUN(conditional);
    RUN(copy);
    printf("==============\nAll tests passed.\n");
    return 0;
}
