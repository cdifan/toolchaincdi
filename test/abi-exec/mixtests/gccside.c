/* Level 3 on OS-9 itself, mixed: this side is compiled by GCC with
   -mos9call, the other (mwside.c) by Microware C 3.2; see there.  main
   calls the Microware functions and checks what they return, reports what
   they saw of their arguments, and lets Microware code call back the
   functions here.  It prints each failure and exits with their number in
   the low byte of the status and 1 in the high byte (Error #001:NNN), so
   that one failure doesn't give a status of 1, which the OS-9 shell
   doesn't report; through exit(), since Microware's cstart.r ignores what
   main returns.

   No libgcc: doubles and floats are compared bit by bit, so that the GCC
   object links with Microware's l68 on its own.  Lines end in '\r', OS-9's
   end of line in either compiler.  */

struct pair { int a; int b; };
struct big { int v[10]; };

extern int printf (const char *, ...);
extern void exit (int);

/* The Microware functions, with the types C 3.2 widens them to where it
   matters (a float argument is a double); char and short arguments are
   widened by the caller under -mos9call anyway.  */
extern int m01 (int, int);
extern int m02 (int, int, int);
extern int m03 (int, int, int, int, int);
extern int m04 (int, double, int);
extern int m05 (double, int, int);
extern int m06 (double, double);
extern int m07 (int, double);
extern int m08 (char *, char *, char *);
extern int m09 (char, short, unsigned char, unsigned short);
#ifdef MISMATCH_FLOAT
/* For check-os9mix: a float argument without the widening C 3.2 expects
   (passed as a float in d0, where m10 wants a double in d0:d1), which the
   check of m10's arguments has to catch, and nothing else.  */
extern int m10 (float, int);
#else
extern int m10 (double, int);
#endif
extern int m11 (struct pair, int);
extern int m12 (int, struct pair, int);
extern int m13 (struct big, int);
extern int m14 (int, int, struct pair);
extern char m15 (void);
extern short m16 (void);
extern unsigned char m17 (void);
extern int *m18 (void);
extern float m19 (void);
extern double m20 (void);
extern long m21 (void);

extern int mwfails;
extern int mwcalls (int (*) (int, int));

static int failures;

static void
check (int ok, const char *what)
{
  if (!ok)
    {
      printf ("FAIL: %s\r", what);
      failures++;
    }
}

static int
dbl_is (double x, unsigned long hi, unsigned long lo)
{
  union { double d; unsigned long w[2]; } u;

  u.d = x;
  return u.w[0] == hi && u.w[1] == lo;
}

static int
flt_is (float x, unsigned long w)
{
  union { float f; unsigned long w; } u;

  u.f = x;
  return u.w == w;
}

/* Called by Microware code.  */

int
g01 (int a, int b)
{
  return a + b * 10;
}

int
g03 (int a, int b, int c, int d, int e)
{
  return a + b + c + d + e;
}

int
g04 (int a, double x, int b)
{
  return a == 1 && dbl_is (x, 0x40040000, 0) && b == 3 ? 4 : -1;
}

/* A float variable from Microware code, which C 3.2 widens to a double
   (_T$FtoD): declared double here, as for any K&R caller (§4 "Widened
   prototypes"); after an int, on the stack, and first, in d0:d1.  */
int
g10 (int a, double x, int b)
{
  return a == 1 && dbl_is (x, 0x40040000, 0) && b == 3 ? 10 : -1;
}

int
g12 (double x, int b)
{
  return dbl_is (x, 0xC0040000, 0) && b == 7 ? 12 : -1;
}

int
g09 (char c, short s, unsigned char uc, unsigned short us)
{
  return c == -3 && s == -1000 && uc == 200 && us == 60000 ? 9 : -1;
}

int
g11 (struct pair p, int a)
{
  return p.a == 11 && p.b == 12 && a == 13 ? 11 : -1;
}

char
g15 (void)
{
  return -2;
}

int *
g18 (void)
{
  static int v = 18;

  return &v;
}

float
g19 (void)
{
  return 1.5f;
}

double
g20 (void)
{
  return -2.25;
}

/* Variadic, called by K&R code with every argument in its place.  */
int
gsum (int n, ...)
{
  __builtin_va_list ap;
  int sum = 0;

  __builtin_va_start (ap, n);
  while (n-- > 0)
    sum += __builtin_va_arg (ap, int);
  __builtin_va_end (ap);
  return sum;
}

int
main (void)
{
  static struct pair p11 = { 11, 12 }, p12 = { 2, 3 }, p14 = { 3, 4 };
  static const char *const mwchecks[24] = {
    0, "g01 int, int", 0, "g03 five ints", "g04 int, double, int", 0, 0, 0, 0,
    "g09 char, short, unsigned char, unsigned short",
    "g10 int, float variable, int", "g11 struct, int",
    "g12 float variable, int", 0, 0, "g15 char return", 0, 0, "g18 pointer return", "g19 float return",
    "g20 double return", 0, "gsum variadic", "g01 through a pointer"
  };
  struct big g;
  int i, fails;

  for (i = 0; i < 10; i++)
    g.v[i] = 100 + i;

  /* GCC calling Microware: the values returned.  */
  check (m01 (1, 2) == 21, "m01 int, int");
  check (m02 (1, 2, 3) == 2, "m02 three ints");
  check (m03 (1, 2, 3, 4, 5) == 15, "m03 five ints");
  check (m04 (1, 2.5, 3) == 4, "m04 int, double, int");
  check (m05 (-0.5, 1, 2) == 5, "m05 double, int, int");
  check (m06 (1.25, 1e10) == 6, "m06 double, double");
  check (m07 (7, 3.0) == 7, "m07 int, double");
  check (m08 ("p", "q", "r") == 8, "m08 three pointers");
  check (m09 (-3, -1000, 200, 60000) == 9, "m09 char, short, unsigned char, unsigned short");
  check (m10 (0.75, 10) == 10, "m10 float, int");
  check (m11 (p11, 13) == 11, "m11 struct, int");
  check (m12 (1, p12, 4) == 12, "m12 int, struct, int");
  check (m13 (g, 14) == 13, "m13 big struct, int");
  check (m14 (1, 2, p14) == 14, "m14 int, int, struct");
  check (m15 () == -2, "m15 char return");
  check (m16 () == -30000, "m16 short return");
  check (m17 () == 250, "m17 unsigned char return");
  check (*m18 () == 18, "m18 pointer return");
  check (flt_is (m19 (), 0x3FC00000), "m19 float return");
  check (dbl_is (m20 (), 0xC0020000, 0), "m20 double return");
  check (m21 () == 123456789L, "m21 long return");

  /* GCC calling Microware: the arguments as they arrived.  */
  for (i = 1; i <= 14; i++)
    if (mwfails & (1 << i))
      {
	printf ("FAIL: arguments as m%02d saw them\r", i);
	failures++;
      }

  /* Microware calling GCC.  */
  fails = mwcalls (g01);
  for (i = 0; i < 24; i++)
    if (fails & (1 << i))
      check (0, mwchecks[i] ? mwchecks[i] : "mwcalls");

  exit (failures != 0 ? 0x100 | failures : 0);
}
