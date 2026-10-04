/* OS-9 itself (make check-os9math): the math functions, which with
   -mos9math are inline traps to OS-9's math module (newlib's
   <machine/os9math.h>), otherwise libm's.  Each result must be within
   1E-13 (relative) of the true value; the math module works to 1E-14.
   Prints each failure; exits with their number.  Errors (sqrt(-1), ...)
   aren't tried: with the math module they end the program (TRAPV).  */

#include <math.h>
#include <stdio.h>

static volatile double half = 0.5, two = 2.0, hundred = 100.0, ten = 10.0;
static int fails;

static void
check (const char *what, double got, double want)
{
  double error = got - want;

  if (error < 0)
    error = -error;
  if (want < 0)
    want = -want;
  if (!(error <= want * 1E-13))
    {
      printf ("FAIL: %s = %.17g, not %.17g\n", what, got, want);
      fails++;
    }
}

int
main (void)
{
  check ("sin (0.5)", sin (half), 0.47942553860420300027);
  check ("cos (0.5)", cos (half), 0.87758256189037271612);
  check ("tan (0.5)", tan (half), 0.54630248984379051326);
  check ("asin (0.5)", asin (half), 0.52359877559829887308);
  check ("acos (0.5)", acos (half), 1.04719755119659774615);
  check ("atan (0.5)", atan (half), 0.46364760900080611621);
  check ("log (2)", log (two), 0.69314718055994530942);
  check ("log10 (100)", log10 (hundred), 2.0);
  check ("sqrt (2)", sqrt (two), 1.41421356237309504880);
  check ("exp (0.5)", exp (half), 1.64872127070012814685);
  check ("pow (2, 10)", pow (two, ten), 1024.0);
  check ("pow (2, 0.5)", pow (two, half), 1.41421356237309504880);
#ifdef __OS9MATH__
  printf ("inline math traps\n");
#endif
  printf ("%d failures\n", fails);
  return fails;
}
