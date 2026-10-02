/*
 * testos9c.c - calling convention probe for Microware C (OS-9/68000)
 *
 * Purpose
 * -------
 * We are adding an -mos9call option to the m68k-elfos9 GCC backend
 * (src/gcc/gcc/config/m68k) so GCC code can call, and be called by,
 * code built with Microware C.  The Microware "OS-9 C Language User
 * Manual", chapter 3, documents the basics:
 *
 *   - 1st integral argument (int/long/pointer, char/short widened)
 *     in d0, 2nd in d1
 *   - a double as the 1st argument in d0:d1 (most significant half
 *     in d0)
 *   - remaining arguments pushed on the stack as 4-byte longwords
 *   - int/float return in d0, double return in d0:d1
 *   - callees preserve every register except the return register(s)
 *     and the register(s) that carried arguments
 *
 * The manual leaves several cases open.  This file exercises them so
 * the generated assembly shows what the compiler really does.
 *
 * How to use
 * ----------
 * This file is plain K&R C on purpose (no prototypes, no // comments).
 * Compile it to assembly only (-a), e.g. with the native Microware C
 * 3.2 compiler on OS-9:
 *
 *     cc -a testos9c.c        -> testos9c.a
 *
 * (-o additionally turns the optimizer off, if that's of interest.)
 *
 * Two outputs are kept next to this file:
 *
 *   testos9c-cc32.a   Microware C 3.2, the original compiler (the
 *                     DOS-hosted cross version, run under vDos):
 *                         xcc -a -s -bp testos9c.c
 *   testos9c-ucc25.a  Microware Ultra C 2.5 in K&R compatibility mode:
 *                         xcc -mode=compat -to=osk -tp=68k -a -c -s testos9c.c
 *
 * Nothing needs to link or run; the x* functions are deliberately
 * left undefined so their call sites can be inspected.
 *
 * If the compiler rejects struct arguments, add -dOS9C_NO_STRUCT_ARGS.
 * If it rejects enums (test 76), add -dOS9C_NO_ENUM.
 * Both compilers accept struct returns (24-27), but they implement
 * them differently.
 *
 * Naming
 * ------
 *   xNN_*     external, never defined: inspect the CALLER side
 *   callNN_*  calls xNN_*: where does the caller put each argument,
 *             and who pops the stack afterwards?
 *   defNN_*   defined here: inspect the CALLEE side (where it reads
 *             arguments, what it saves, how it returns)
 *   keepNN_*  values live across a call: which registers does the
 *             caller assume survive the call?
 * Every name is unique in its first 6 characters in case the
 * toolchain truncates symbols.
 *
 * Questions this file answers (by test number)
 * -------------------------------------------
 *   01-03  baseline integral args, many args, stack push order, cleanup
 *   04     int, double, int: does the 3rd arg go in d1 or on the stack?
 *   05-07  double placement (1st, 2nd, both, alone)
 *   08     pointers count as integral (d0/d1, not a0/a1)?
 *   09     char/short/unsigned widening in registers and on the stack
 *   10-11  float arguments: always promoted to double?
 *   12-15  struct by value: on the stack?  do later ints still get d0/d1?
 *   16     call through a function pointer: same convention?
 *   17-23  return values: who widens char/short?  float vs double?
 *          pointer in d0 or a0?
 *   24-27  struct return: hidden buffer pointer (in a0?), do arguments
 *          still use d0/d1, and how does the callee fill the buffer?
 *   28-29  float return: does a float function return a float in d0
 *          or a double in d0:d1?  (callee side, and a caller storing
 *          the result into a float)
 *   30-36  callee side: what registers does a callee save?
 *   40-47  strict preservation rule: does a caller rely on d0/d1
 *          (and a0/a1, d2/d3) surviving a call when they carried no
 *          argument and no return value?  Does a callee save them?
 *   50-62  odd-sized struct arguments (1, 3, 5 and 7 bytes): sizeof,
 *          padding and slot rounding on the stack, and where the
 *          arguments after such a struct end up
 *   70-83  data layout: member alignment (offsets and sizeof of
 *          {char; int}, {char; short}, {char; double}), enum size,
 *          bitfield size and bit order, and 2- and 6-byte structs as
 *          arguments
 */

/* ------------------------------------------------------------------ */
/* Globals used as argument sources, so arguments are not constants.  */
/* ------------------------------------------------------------------ */

int gi, gj, gk, gl, gm;
long glong;
double ga, gb;
float gf, gg;
char gc;
short gs;
unsigned char guc;
unsigned short gus;
int *gp, *gq, *gr;

struct pair {
	int lo;
	int hi;
};

struct big {
	int v[5];
};

struct pair gpair;
struct big gbig;

/* ------------------------------------------------------------------ */
/* Caller side: arguments.                                            */
/* ------------------------------------------------------------------ */

/* 01: int, int - baseline from the manual: expect d0, d1 */
extern int x01_int_int();
int call01_int_int() { return x01_int_int(gi, gj); }

/* 02: int, int, int - expect d0, d1, 3rd pushed */
extern int x02_int_int_int();
int call02_int_int_int() { return x02_int_int_int(gi, gj, gk); }

/* 03: five ints - push order (right to left?) and who pops (caller?) */
extern int x03_five_ints();
int call03_five_ints() { return x03_five_ints(gi, gj, gk, gl, gm); }

/* 04: int, double, int - OPEN QUESTION: int in d0, double on the stack,
   but does the trailing int go in d1 or on the stack? */
extern int x04_int_dbl_int();
int call04_int_dbl_int() { return x04_int_dbl_int(gi, ga, gj); }

/* 05: double, int, int - expect double in d0:d1, both ints pushed */
extern int x05_dbl_int_int();
int call05_dbl_int_int() { return x05_dbl_int_int(ga, gi, gj); }

/* 06: double, double - expect 1st in d0:d1, 2nd pushed (8 bytes) */
extern int x06_dbl_dbl();
int call06_dbl_dbl() { return x06_dbl_dbl(ga, gb); }

/* 07: int, double - manual: int in d0, double on the stack, d1 unused */
extern int x07_int_dbl();
int call07_int_dbl() { return x07_int_dbl(gi, ga); }

/* 08: pointer, pointer, pointer - pointers are "integral": d0, d1,
   pushed (and not a0/a1) */
extern int x08_ptr_ptr_ptr();
int call08_ptr_ptr_ptr() { return x08_ptr_ptr_ptr(gp, gq, gr); }

/* 09: char, short, unsigned char, unsigned short - check sign/zero
   extension in d0/d1 and on the stack */
extern int x09_chr_sht_uch_ush();
int call09_chr_sht_uch_ush() { return x09_chr_sht_uch_ush(gc, gs, guc, gus); }

/* 10: float, int - is the float promoted to double and put in d0:d1
   (so the int is pushed)? */
extern int x10_flt_int();
int call10_flt_int() { return x10_flt_int(gf, gi); }

/* 11: int, float - float promoted to double and pushed as 8 bytes? */
extern int x11_int_flt();
int call11_int_flt() { return x11_int_flt(gi, gf); }

#ifndef OS9C_NO_STRUCT_ARGS
/* 12: struct pair, int - OPEN QUESTION: struct on the stack, and does
   the int then become the "first integral argument" in d0? */
extern int x12_pair_int();
int call12_pair_int() { return x12_pair_int(gpair, gi); }

/* 13: int, struct pair, int - OPEN QUESTION: does the 2nd int go in d1
   after a stack struct? */
extern int x13_int_pair_int();
int call13_int_pair_int() { return x13_int_pair_int(gi, gpair, gj); }

/* 14: struct big, int - larger struct: copied onto the stack, or passed
   by hidden pointer? */
extern int x14_big_int();
int call14_big_int() { return x14_big_int(gbig, gi); }

/* 15: int, int, struct pair - struct after both registers are used */
extern int x15_int_int_pair();
int call15_int_int_pair() { return x15_int_int_pair(gi, gj, gpair); }
#endif

/* 16: call through a function pointer (int, int, int) - same as 02? */
int (*gfnptr)();
int call16_indirect() { return (*gfnptr)(gi, gj, gk); }

/* ------------------------------------------------------------------ */
/* Caller side: return values.                                        */
/* ------------------------------------------------------------------ */

/* 17: char return - does the CALLER widen d0 (ext.w/ext.l) or trust
   the callee to have done it? */
extern char x17_ret_char();
int call17_ret_char() { return x17_ret_char() + 1; }

/* 18: short return - same question */
extern short x18_ret_short();
int call18_ret_short() { return x18_ret_short() + 1; }

/* 19: unsigned char return - zero extension by caller? */
extern unsigned char x19_ret_uchar();
int call19_ret_uchar() { return x19_ret_uchar() + 1; }

/* 20: pointer return - read from d0 (expected) or a0? */
extern int *x20_ret_ptr();
int call20_ret_ptr() { return *x20_ret_ptr(); }

/* 21: float return - read as a float in d0, or as a double in d0:d1? */
extern float x21_ret_float();
double call21_ret_float() { return x21_ret_float(); }

/* 22: double return - expect d0:d1 */
extern double x22_ret_double();
double call22_ret_double() { return x22_ret_double() + 1.0; }

/* 23: long return - same as int (long is 32 bits) */
extern long x23_ret_long();
long call23_ret_long() { return x23_ret_long() + 1; }

/* 24: small (8-byte) struct return, no arguments - returned in d0:d1,
   or through a buffer whose address the caller passes (in a0?) */
extern struct pair x24_ret_pair();
int call24_ret_pair() { struct pair p; p = x24_ret_pair(); return p.hi; }

/* 25: large struct return with two int arguments - do the arguments
   still go in d0/d1 next to the hidden buffer pointer? */
extern struct big x25_ret_big();
int call25_ret_big(i)
int i;
{
	struct big b;

	b = x25_ret_big(i, gj);
	return b.v[4];
}

/* 26: callee side of 24 with two int arguments - where does it put
   the result, and does it return the buffer address in d0? */
struct pair def26_ret_pair(i, j)
int i, j;
{
	struct pair p;

	p.lo = i;
	p.hi = j;
	return p;
}

/* 27: callee side of 25 - copying a large struct into the buffer */
struct big def27_ret_big(i)
int i;
{
	gbig.v[0] = i;
	return gbig;
}

/* 28: callee returning float - is the result a float in d0, or a
   double in d0:d1 (K&R style)?  Compare with the caller in test 21. */
float def28_ret_float(i)
int i;
{
	return gf * i;
}

/* 29: caller storing a float function's result into a float variable -
   does it convert from double, or store d0 as it is? */
void call29_ret_float_to_float()
{
	gg = x21_ret_float();
}

/* ------------------------------------------------------------------ */
/* Callee side: where arguments are read from, what gets saved.       */
/* ------------------------------------------------------------------ */

/* 30: int, double, int - where does the callee read k from?  Must
   agree with test 04. */
int def30_int_dbl_int(i, a, k)
int i;
double a;
int k;
{
	gi = i;
	ga = a;
	return k;
}

#ifndef OS9C_NO_STRUCT_ARGS
/* 31: struct pair, int - where does the callee read i from?  Must
   agree with test 12. */
int def31_pair_int(p, i)
struct pair p;
int i;
{
	return p.hi + i;
}
#endif

/* 32: float parameter - is it really received as a double?  Look at
   how many bytes the callee reads and where from. */
int def32_flt_int(f, i)
float f;
int i;
{
	gf = f;
	return i;
}

/* 33: char/short parameters - does the callee re-extend, or trust the
   caller's widening? */
int def33_chr_sht(c, s)
char c;
short s;
{
	return c + s;
}

/* 34: char return - does the callee widen d0 before returning?
   Together with 17 this shows who is responsible. */
char def34_ret_char(i)
int i;
{
	return (char)(i + gi);
}

/* 35: callee that clobbers a0/a1 - does it save/restore them
   (movem)?  The manual says it must. */
int def35_uses_a0_a1(p, q)
int *p, *q;
{
	return p[0] + q[1] + p[2] + q[3];
}

/* 36: non-leaf callee with live temporaries across a call - which
   registers does it save in the prologue? */
int def36_nonleaf(p, i)
int *p;
int i;
{
	int t;

	t = p[0] + i;
	t += x01_int_int(p[1], i);
	return t + p[2];
}

/* ------------------------------------------------------------------ */
/* Strict preservation rule for d0/d1.                                */
/*                                                                    */
/* The manual says a function may only clobber its return register(s) */
/* and the register(s) its arguments came in.  Taken literally, a     */
/* call to "void f()" with no arguments must preserve d0 AND d1.      */
/*                                                                    */
/*   40-42 (caller): does the compiler keep a value in d0/d1 across   */
/*         such a call and use it afterwards without reloading?       */
/*         If yes, GCC callees must save d0/d1 when they are unused.  */
/*   43-45 (callee): do Microware callees actually save d0/d1 when    */
/*         those carried no argument and no return value?             */
/* ------------------------------------------------------------------ */

extern void x40_void_noargs();
extern void x41_void_one_int();
extern int x42_ret_int_noargs();

/* 40: i and j arrive in d0/d1, then a no-arg void call.  After the
   call, is i + j computed straight from d0/d1 (caller relies on the
   strict rule) or from copies saved elsewhere? */
int keep40_d0d1_across_void_noargs(i, j)
int i, j;
{
	x40_void_noargs();
	return i + j;
}

/* 41: as 40, but the call takes one int (in d0).  Expect d0 to be
   treated as clobbered; is d1 (j) still trusted after the call? */
int keep41_d1_across_void_one_int(i, j)
int i, j;
{
	x41_void_one_int(5);
	return i + j;
}

/* 42: as 40, but the call returns int in d0.  Is d1 (j) trusted after
   the call? */
int keep42_d1_across_ret_int(i, j)
int i, j;
{
	int r;

	r = x42_ret_int_noargs();
	return r + i + j;
}

/* 43: callee with no arguments and no return value that needs scratch
   data registers (32-bit multiply/divide calls a library helper on
   the 68000).  Does it save d0/d1 in the prologue? */
void def43_void_noargs_uses_d0d1()
{
	gi = gj * gk + gl / gk;
}

/* 44: one int argument (d0), int return (d0), but d1 needed as
   scratch.  Does it save d1? */
int def44_one_int_uses_d1(i)
int i;
{
	return i * gj + gk / i;
}

/* 45: double return (d0:d1) - here d1 is a return register; check
   that nothing extra is saved, as a control for 43/44. */
double def45_ret_double(i)
int i;
{
	return ga * i;
}

/* 46: pointers arrive in d0/d1 and are probably copied to a0/a1.
   After a call, are a0/a1 trusted (caller relies on callees
   preserving a0/a1) or reloaded? */
int keep46_a0a1_across_call(p, q)
int *p, *q;
{
	int t;

	t = *p + *q;
	x40_void_noargs();
	return t + *p + *q;
}

/* 47: temporaries the manual calls "compiler allocated" (d2/d3,
   a0/a1) - are they kept live across a call? */
int keep47_temps_across_call(p, i)
int *p;
int i;
{
	return p[0] * i + x42_ret_int_noargs() + p[1] * i;
}

#ifndef OS9C_NO_STRUCT_ARGS
/* ------------------------------------------------------------------ */
/* Odd-sized struct arguments.                                        */
/*                                                                    */
/* The struct tests above only use 8- and 20-byte structs.  These     */
/* show how structs of 1, 3, 5 and 7 bytes are sized, padded and      */
/* placed on the stack, and where the arguments after them go.        */
/* ------------------------------------------------------------------ */

struct odd1 { char c[1]; };
struct odd3 { char c[3]; };
struct odd5 { char c[5]; };
struct odd7 { char c[7]; };

struct odd1 godd1;
struct odd3 godd3;
struct odd5 godd5;
struct odd7 godd7;

/* 50-53: sizeof - does the compiler pad these structs (e.g. to an even
   size)? */
int size50_odd1() { return sizeof(struct odd1); }
int size51_odd3() { return sizeof(struct odd3); }
int size52_odd5() { return sizeof(struct odd5); }
int size53_odd7() { return sizeof(struct odd7); }

/* 54-57: struct oddN, int - how many bytes does the struct take on the
   stack, where within its slot is it placed, and where does the int go
   (stack, as for 8-byte structs in test 12)? */
extern int x54_odd1_int();
int call54_odd1_int() { return x54_odd1_int(godd1, gi); }

extern int x55_odd3_int();
int call55_odd3_int() { return x55_odd3_int(godd3, gi); }

extern int x56_odd5_int();
int call56_odd5_int() { return x56_odd5_int(godd5, gi); }

extern int x57_odd7_int();
int call57_odd7_int() { return x57_odd7_int(godd7, gi); }

/* 58-61: int, struct oddN, int - the first int in d0, then the struct
   and the last int on the stack (as in test 13)?  Shows the slot
   rounding between the struct and the following int. */
extern int x58_int_odd1_int();
int call58_int_odd1_int() { return x58_int_odd1_int(gi, godd1, gj); }

extern int x59_int_odd3_int();
int call59_int_odd3_int() { return x59_int_odd3_int(gi, godd3, gj); }

extern int x60_int_odd5_int();
int call60_int_odd5_int() { return x60_int_odd5_int(gi, godd5, gj); }

extern int x61_int_odd7_int();
int call61_int_odd7_int() { return x61_int_odd7_int(gi, godd7, gj); }

/* 62: callee side of 55 - at which offsets does the callee read the
   3-byte struct's bytes and the int after it? */
int def62_odd3_int(p, i)
struct odd3 p;
int i;
{
	return p.c[0] + p.c[2] + i;
}
#endif

/* ------------------------------------------------------------------ */
/* Data layout.                                                       */
/*                                                                    */
/* Structs are often shared with Microware-compiled code through      */
/* pointers, so their layout must match.  These show member           */
/* alignment, enum size and bitfield order.                           */
/* ------------------------------------------------------------------ */

struct lay_ci { char c; int i; };
struct lay_cs { char c; short s; };
struct lay_cd { char c; double d; };
struct lay_bf { unsigned a:3; unsigned b:5; unsigned c:9; };

struct lay_bf gbf;

/* 70-75: member offsets and sizes - is an int (or double) after a char
   aligned to 2 or to 4 bytes? */
int offs70_ci_i() { return (int)&((struct lay_ci *)0)->i; }
int size71_ci() { return sizeof(struct lay_ci); }
int offs72_cs_s() { return (int)&((struct lay_cs *)0)->s; }
int size73_cs() { return sizeof(struct lay_cs); }
int offs74_cd_d() { return (int)&((struct lay_cd *)0)->d; }
int size75_cd() { return sizeof(struct lay_cd); }

#ifndef OS9C_NO_ENUM
/* 76: enum size - int (4 bytes), or smaller? */
enum lay_en { EN_A, EN_B };
int size76_enum() { return sizeof(enum lay_en); }
#endif

/* 77: bitfield struct size (3 + 5 + 9 bits) */
int size77_bf() { return sizeof(struct lay_bf); }

/* 78-79: bitfield bit order - which bits of which byte/word does
   setting b (bits 3-7 of the first unit?) and c (crossing the byte
   boundary?) change? */
void set78_bf_b() { gbf.b = 1; }
void set79_bf_c() { gbf.c = 1; }

#ifndef OS9C_NO_STRUCT_ARGS
struct odd2 { char c[2]; };
struct odd6 { char c[6]; };

struct odd2 godd2;
struct odd6 godd6;

/* 80-81: sizeof the 2- and 6-byte structs */
int size80_odd2() { return sizeof(struct odd2); }
int size81_odd6() { return sizeof(struct odd6); }

/* 82: struct odd2, int - slot size and placement of a 2-byte struct */
extern int x82_odd2_int();
int call82_odd2_int() { return x82_odd2_int(godd2, gi); }

/* 83: int, struct odd6, int - slot size and placement of a 6-byte
   struct between two ints */
extern int x83_int_odd6_int();
int call83_int_odd6_int() { return x83_int_odd6_int(gi, godd6, gj); }
#endif
