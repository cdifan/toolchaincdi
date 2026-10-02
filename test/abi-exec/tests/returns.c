/* Level 2, return values: GCC -mos9call code calling Microware-style
   assembly (returns.S) and called by it.  main returns a bit mask of
   the failed checks.  */

struct s3i { int a, b, c; };
struct s1 { char c; };
union fw { float f; unsigned long w; };
union dw { double d; unsigned long w[2]; };

extern float mw_retf (void);
extern double mw_retd (void);
extern struct s3i mw_rets (int);
extern struct s1 mw_rets1 (int);
extern long call_g_rets (void);
extern long call_g_retf (void);

struct s3i g_rets (int x) { struct s3i r = { x, x * 2, x * 3 }; return r; }
float g_retf (void) { return 1.5f; }

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  union fw f;
  union dw d;
  struct s3i s;
  struct s1 s1;

  f.f = mw_retf ();
  check (f.w == 0x3FC00000, 0);
  d.d = mw_retd ();
  check (d.w[0] == 0x40040000 && d.w[1] == 0, 1);
  s = mw_rets (10);
  check (s.a == 10 && s.b == 11 && s.c == 12, 2);
  s1 = mw_rets1 (42);
  check (s1.c == 42, 3);
  check (call_g_rets () == 5 + 10 + 15, 4);
  check (call_g_retf () == 0x3FC00000, 5);
  return fails;
}
