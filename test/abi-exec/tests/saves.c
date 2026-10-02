/* Level 2, register preservation: GCC -mos9call functions called the
   Microware way (saves.S) must preserve d1 (unless it carries an
   argument or the return value), a0, a1 and d2-d7/a2-a5.  main returns
   a bit mask of the failed tests.  */

typedef __SIZE_TYPE__ size_t;
extern long mwcall (void *, long, long, int);
extern long mw_result, mw_result2;
extern void __attribute__ ((stackcall)) clobber (void);
volatile int divisor = 7;

#define D1_FREE 1		/* d1 must survive */

/* Leaf, one argument: d1 free.  */
int leaf1 (int a) { return a + 1; }

/* Leaf with a pointer argument and a loop: uses a0 and d1 (an
   argument here).  */
int sum (int *p, int n) { int s = 0; while (n--) s += *p++; return s; }

/* Leaf with pointer work but one argument: a0/a1 must be saved.  */
int copy4 (int *p) { int *q = p + 4; q[0] = p[0]; q[1] = p[1]; return q[0] + q[1]; }

/* Non-leaf: the stack-convention callee destroys d1, a0 and a1.  */
int nonleaf1 (int a) { clobber (); return a; }
int nonleaf2 (int a, int b) { clobber (); return a + b; }

/* Libcalls (division) also destroy d1, a0 and a1.  */
int divide (int a) { return a / divisor; }

/* A long long argument and result occupy d0:d1.  */
long long ladd (long long x) { clobber (); return x + 1; }

/* A long long result occupies d1.  */
long long ret64 (int a) { clobber (); return (long long) a << 32 | 5; }

/* memcpy for a struct copy (stack convention).  */
struct big { int a[50]; };
int bigcopy (struct big *p) { struct big t = *p; clobber (); return t.a[49]; }

/* A frame of 32 KB or more: the epilogue indexes with a1.  */
int bigframe (int a) { volatile char buf[40000]; buf[39999] = a; clobber (); return buf[39999]; }

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  int v[8] = { 1, 2, 3, 4, 0, 0, 0, 0 };
  struct big b;
  long m;

  m = mwcall (leaf1, 5, 0, D1_FREE);
  check (m == 0 && mw_result == 6, 0);
  m = mwcall (sum, (long) v, 4, 0);
  check (m == 0 && mw_result == 10, 1);
  m = mwcall (copy4, (long) v, 0, D1_FREE);
  check (m == 0 && mw_result == 3 && v[4] == 1, 2);
  m = mwcall (nonleaf1, 7, 0, D1_FREE);
  check (m == 0 && mw_result == 7, 3);
  m = mwcall (nonleaf2, 7, 8, 0);
  check (m == 0 && mw_result == 15, 4);
  m = mwcall (divide, 49, 0, D1_FREE);
  check (m == 0 && mw_result == 7, 5);
  m = mwcall (ret64, 9, 0, 0);
  check (m == 0 && mw_result == 9 && mw_result2 == 5, 6);
  b.a[49] = 4242;
  m = mwcall (bigcopy, (long) &b, 0, D1_FREE);
  check (m == 0 && mw_result == 4242, 7);
  m = mwcall (bigframe, 99, 0, D1_FREE);
  check (m == 0 && mw_result == 99, 8);
  /* ladd: x = 5 in d0:d1 (d1 carries the argument and the result).  */
  m = mwcall (ladd, 0, 5, 0);
  check (m == 0 && mw_result == 0 && mw_result2 == 6, 9);
  return fails;
}
