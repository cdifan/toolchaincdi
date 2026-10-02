/* Level 2, caller side of the OS-9 convention: callers keep values in a0
   and a1 across calls to OS-9 convention functions, which preserve them
   (keep.S: os9_inc), but not across stack convention calls, which may
   clobber them (stack_trash clobbers d1, a0 and a1), nor libcalls.
   main returns a bit mask of the failed checks.  */

extern int os9_inc (int);
extern int stack_trash (int) __attribute__ ((stackcall));
int (*volatile pf) (int) = os9_inc;
/* a0 and a1 at os9_inc's last call, read as volatile integers: the stub
   stores them, which GCC doesn't see (as pointers, it would fold the
   comparisons with the local v[] to false).  */
extern volatile unsigned long seen_a0, seen_a1;
static int
seen (unsigned long r, const int *v)
{
  return (r == (unsigned long) &v[0] || r == (unsigned long) &v[1]
	  || r == (unsigned long) &v[2]);
}

static int __attribute__ ((noinline))
keep_os9 (int *p, int *q, int *r)
{
  int a = os9_inc (*p);
  a += os9_inc (*q);
  a += pf (*r);
  return a + *p + *q + *r;
}

static int __attribute__ ((noinline))
keep_stack (int *p, int *q, int *r)
{
  int a = stack_trash (*p);
  a += stack_trash (*q);
  a += stack_trash (*r);
  return a + *p + *q + *r;
}

static long long __attribute__ ((noinline))
keep_libcall (long long *p, long long x)
{
  long long y = x * p[0];
  return y * p[1] + p[0];
}

int
main (void)
{
  int fail = 0;
  int v[3] = { 10, 200, 3000 };
  long long w[2] = { 3, 0x100000001LL };

  if (keep_os9 (&v[0], &v[1], &v[2]) != 2 * (10 + 200 + 3000) + 3)
    fail |= 1;
#ifdef __OPTIMIZE__
  /* The point of the test: with optimization, the caller keeps one of
     the pointers live across the calls in a0 or a1.  */
  if (!seen (seen_a0, v) && !seen (seen_a1, v))
    fail |= 8;
#endif
  if (keep_stack (&v[0], &v[1], &v[2]) != 2 * (10 + 200 + 3000) + 3)
    fail |= 2;
  if (keep_libcall (w, 5) != 15 * 0x100000001LL + 3)
    fail |= 4;
  return fail;
}
