/* Level 2, variadic functions defined with -mos9call: called the way
   Microware C 3.2 code calls them (varargs.S), and by GCC code.  main
   returns a bit mask of the failed checks.  */

#include <stdarg.h>

union dw { double d; unsigned long w[2]; };

extern int call_sum (void), call_mix_idi (void), call_mix_di (void);
extern int call_two (void), call_dfirst (void);

/* n ints.  */
int
sum (int n, ...)
{
  va_list ap;
  int s = 0;
  va_start (ap, n);
  while (n--)
    s += va_arg (ap, int);
  va_end (ap);
  return s;
}

/* Arguments described by FMT: i = int, d = double (its high word is
   added, plus its low word).  */
int
mix (const char *fmt, ...)
{
  va_list ap;
  int s = 0;
  union dw u;
  va_start (ap, fmt);
  for (; *fmt; fmt++)
    if (*fmt == 'i')
      s += va_arg (ap, int);
    else
      {
	u.d = va_arg (ap, double);
	s += (int) (u.w[0] >> 16) + (int) u.w[1];
      }
  va_end (ap);
  return s;
}

/* Two named arguments: every unnamed one is on the stack.  */
int
two (int a, int b, ...)
{
  va_list ap;
  int s;
  va_start (ap, b);
  s = a + b + va_arg (ap, int);
  s += va_arg (ap, int);
  va_end (ap);
  return s;
}

/* A double first argument (d0:d1): the rest on the stack.  */
int
dfirst (double x, ...)
{
  va_list ap;
  union dw u;
  int s;
  u.d = x;
  va_start (ap, x);
  s = (int) (u.w[0] >> 16) + va_arg (ap, int);
  s += va_arg (ap, int);
  va_end (ap);
  return s;
}

/* va_start again after va_end, and va_copy after d1 was used.  */
int
again (int n, ...)
{
  va_list ap, ap2;
  int a, b, c, d;
  va_start (ap, n);
  a = va_arg (ap, int);
  va_copy (ap2, ap);
  b = va_arg (ap, int);
  c = va_arg (ap2, int);
  va_end (ap2);
  va_end (ap);
  va_start (ap, n);
  d = va_arg (ap, int);
  va_end (ap);
  /* a and d are the first unnamed argument (in d1), b and c the second
     (on the stack).  */
  return a == d && b == c ? a * 100 + b : -1;
}

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  /* Called the Microware way.  */
  check (call_sum () == 60, 0);
  check (call_mix_idi () == 7 + 0x3FF8 + 9, 1);
  check (call_mix_di () == 0x3FF8 + 9, 2);
  check (call_two () == 10, 3);
  check (call_dfirst () == 0x3FF8 + 15, 4);
  /* Called by GCC code.  */
  check (sum (3, 10, 20, 30) == 60, 5);
  check (mix ("idi", 7, 1.5, 9) == 7 + 0x3FF8 + 9, 6);
  check (mix ("di", 1.5, 9) == 0x3FF8 + 9, 7);
  check (two (1, 2, 3, 4) == 10, 8);
  check (dfirst (1.5, 7, 8) == 0x3FF8 + 15, 9);
  check (again (2, 11, 22) == 1122, 10);
  return fails;
}
