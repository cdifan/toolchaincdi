/* Level 2, argument passing: GCC -mos9call code calling Microware C
   3.2-style assembly (args.S), and called by it.  main returns a bit
   mask of the failed checks.  */

struct s1 { char c; };
struct s3 { char a, b, c; };
union dw { double d; long long ll; unsigned long w[2]; };

extern int mw_sub3 (int, int, int);
extern int mw_dhi (double, int);
extern int mw_dlo (double);
extern int mw_s1 (struct s1, int);
extern int mw_s3 (struct s3, int);
extern int mw_idi (int, double, int);
extern int mw_var (int, ...);
extern int mw_ll (long long, int);
extern int call_g_sub3 (void);
extern int call_g_dhi (void);
extern int call_g_s1 (void);
extern int call_g_idi (void);

int g_sub3 (int a, int b, int c) { return a - b - c; }
int g_dhi (double x, int y) { union dw u; u.d = x; return u.w[0] + y; }
int g_s1 (struct s1 s, int i) { return s.c + i; }
int g_idi (int a, double b, int c) { union dw u; u.d = b; return a + u.w[0] + c; }

static unsigned long fails;
static void check (long got, long want, int n) { if (got != want) fails |= 1UL << n; }

int
main (void)
{
  union dw u;
  struct s1 s1 = { 7 };
  struct s3 s3 = { 1, 2, 9 };

  check (mw_sub3 (100, 20, 3), 77, 0);
  check (mw_dhi (1.5, 1), 0x3FF80001, 1);
  u.w[0] = 0x40000000; u.w[1] = 0x12345678;
  check (mw_dlo (u.d), 0x12345678, 2);
  check (mw_s1 (s1, 100), 107, 3);
  check (mw_s3 (s3, 100), 109, 4);
  check (mw_idi (10, 1.5, 5), 10 + 0x3FF80000 + 5, 5);
  check (mw_var (100, 20, 3), 77, 6);
  check (mw_ll (0x100000005LL, 2), 7, 7);
  check (call_g_sub3 (), 77, 8);
  check (call_g_dhi (), 0x3FF80005, 9);
  check (call_g_s1 (), 107, 10);
  check (call_g_idi (), 10 + 0x3FF80000 + 5, 11);
  return fails;
}
