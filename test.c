/* Glyph VM tests */
#define GLYPH_IMPL
#include "glyph.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define TEST(name) static int test_##name(void)
#define RUN(name) printf("%-20s", #name); if (!test_##name()) {printf("OK\n");}
#define ASSERT(x) do { if(!(x)) { printf("FAIL: %s\n", #x); return 1; } } while(0)

static Glyph vm;
static void run(const char *prog) {
	bzero(&vm, sizeof(vm));
	memcpy(vm.m, prog, strlen(prog) + 1);
	glyph_eval(&vm);
}

TEST(arithmetic) {
	run("5=a 3=b +ab=c -ab=d *ab=e /ab=f");
	ASSERT(vm.r['a'] == 5);
	ASSERT(vm.r['b'] == 3);
	ASSERT(vm.r['c'] == 8);
	ASSERT(vm.r['d'] == 2);
	ASSERT(vm.r['e'] == 15);
	ASSERT(vm.r['f'] == 1);
	return 0;
}

TEST(bitwise) {
	run("15=a 7=b &ab=c |ab=d ^ab=e ~a=f");
	ASSERT(vm.r['a'] == 15);
	ASSERT(vm.r['b'] == 7);
	ASSERT(vm.r['c'] == 7);
	ASSERT(vm.r['d'] == 15);
	ASSERT(vm.r['e'] == 8);
	ASSERT(vm.r['f'] == (uint8_t)~15u);
	return 0;
}

TEST(shifts) {
	run("4=a 2<a=c 2>a=d");
	ASSERT(vm.r['a'] == 4);
	ASSERT(vm.r['c'] == 16);
	ASSERT(vm.r['d'] == 1);
	return 0;
}

TEST(memory) {
	run("'*=b '2@>b@<c");
	ASSERT(vm.r['c'] == '*');
	ASSERT(vm.m['2'] == '*');
	return 0;
}

TEST(ports) {
	run("'c=b 5#>b");
	ASSERT(vm.p[5] == 99);
	bzero(&vm, sizeof(vm));
	const char test[] = "10#<b";
	vm.p[10] = 77;
	memcpy(vm.m, test, sizeof(test));
	glyph_eval(&vm);
	ASSERT(vm.r['b'] == 77);
	return 0;
}

TEST(stack) {
	run("34=, 35=, +,,=a");
	ASSERT(vm.r['a'] == 69);
	return 0;
}

TEST(jump) {
	run("13=a a. 34=b 35=a +ab=c");
	ASSERT(vm.r['c'] != 69);
	ASSERT(vm.r['b'] == 0);
	ASSERT(vm.r['a'] == 35);
	return 0;
}

TEST(backward_jump) {
	run("1=c +bc=b 2?=b 23:. z. 10=z +zb=b");
	ASSERT(vm.r['z'] == 10);
	ASSERT(vm.r['b'] == 12);
	return 0;
}

TEST(conditional_eq) {
	run("5=a 5?=a 1=r 9:r");
	ASSERT(vm.r['r'] == 9);
	run("5=a 3?=b 1=r 9:r");
	ASSERT(vm.r['r'] == 1);
	return 0;
}

TEST(conditional_neq) {
	run("5=a 3?!a 1=r 9:r");
	ASSERT(vm.r['r'] == 9);
	run("5=a 5?!a 1=r 9:r");
	ASSERT(vm.r['r'] == 1);
	return 0;
}

TEST(conditional_gt) {
	run("3=a 5?>a 1=r 9:r");
	ASSERT(vm.r['r'] == 9);
	run("5=a 3?>a 1=r 9:r");
	ASSERT(vm.r['r'] == 1);
	return 0;
}

TEST(conditional_lt) {
	run("5=a 3?<a 1=r 9:r");
	ASSERT(vm.r['r'] == 9);
	run("3=a 5?<a 1=r 9:r");
	ASSERT(vm.r['r'] == 1);
	return 0;
}

TEST(call_return) {
	run("12=. 5=r ,. 1=r 5=f ;f 1=i +ri=s");
	ASSERT(vm.r['r'] == 5);
	ASSERT(vm.r['s'] == 6);
	return 0;
}

TEST(nested_calls) {
	/* Two functions: F sets r=3, G calls F then adds 1 */
	run("32=. 3=r ,. 5=f ;f 1=i +ri=r ,. 0=r 12=g ;g");
	ASSERT(vm.r['r'] == 4);  /* G: call F (r=3), r+=1 (r=4) */
	return 0;
}

TEST(copy) {
	run("'*=a ab");
	ASSERT(vm.r['b'] == 42);
	return 0;
}

TEST(labels) {
	run(".a.b.c.d");
	ASSERT(vm.r['a'] == 2);
	ASSERT(vm.r['b'] == 4);
	ASSERT(vm.r['c'] == 6);
	ASSERT(vm.r['d'] == 8);
	return 0;
}

int main(void) {
	printf("Glyph VM Tests\n==============\n");
	RUN(arithmetic);
	RUN(bitwise);
	RUN(shifts);
	RUN(memory);
	RUN(ports);
	RUN(stack);
	RUN(jump);
	RUN(backward_jump);
	RUN(conditional_eq);
	RUN(conditional_neq);
	RUN(conditional_gt);
	RUN(conditional_lt);
	RUN(call_return);
	RUN(nested_calls);
	RUN(copy);
	RUN(labels);
	printf("==============\n");
	return 0;
}
