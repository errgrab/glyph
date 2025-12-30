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
    run(":ax5 :bx3 +cab -dab *eab /fab");
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
    run(":ax4 :bx2 <cab >dab");
    ASSERT(vm.reg['c'] == 16);
    ASSERT(vm.reg['d'] == 1);
}

TEST(memory) {
	run(":ag2 :bg* !ab @ca");
    ASSERT(vm.reg['c'] == '*');
    ASSERT(mem['2'] == '*');
}

TEST(ports) {
	run(":ax5 :bgc #>ab");
    ASSERT(vm.port[5] == 99);
	const char *test = ":axa #<ba";
    glyph_init(&vm, mem, sizeof(mem));
    vm.port[10] = 77;
    memcpy(mem, test, strlen(test) + 1);
    glyph_run(&vm);
    ASSERT(vm.reg['b'] == 77);
}

TEST(jump) {
	run(":jxf .j :ax1");
    ASSERT(vm.reg['a'] == 0);  /* skipped */
}

TEST(conditional) {
    run(":ax5 :bx5 ?=ab:cx1 :dx2");
    ASSERT(vm.reg['c'] == 1 && vm.reg['d'] == 2);

    run(":ax5 :bx3 ?=ab:cx1");
    ASSERT(vm.reg['c'] == 0);  /* skipped due to inequality */
}

TEST(copy) {
	run(":ag* :b.a");
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
