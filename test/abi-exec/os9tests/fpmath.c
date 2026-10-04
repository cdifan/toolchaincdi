/* OS-9 itself (make check-os9math): floating point through GCC's
   soft-float entry points, with results that are exact, so any correct
   implementation gives them bit for bit: libgcc, or with -mos9math
   OS-9's math module (inline traps, and libos9math for what isn't
   inline).  Comparisons both ways, conversions and NaNs, which the math
   module doesn't know (see below).  Prints each failure; exits with their
   number.  */

#include <stdio.h>
#include <string.h>

static volatile double d1 = 1.5, d2 = 2.25, dm = -3.0, dz = 0.0, dbig = 3e9;
static volatile float f1 = 1.5f, f2 = 2.25f, fm = -3.0f;
static volatile long lv = -7;
static volatile unsigned long uv = 3000000000UL;
static int fails;

static void
check (int ok, const char *what)
{
  if (!ok)
    {
      printf ("FAIL: %s\n", what);
      fails++;
    }
}

static int
dis (double x, unsigned long hi, unsigned long lo)
{
  unsigned long w[2];

  memcpy (w, &x, sizeof w);
  return w[0] == hi && w[1] == lo;
}

static int
fis (float x, unsigned long w)
{
  unsigned long v;

  memcpy (&v, &x, sizeof v);
  return v == w;
}

int
main (void)
{
  volatile double nan;
  volatile float fnan;

  /* Doubles: 3.75, -0.75, 3.375, 0.6666... isn't exact, so 2.25 / 1.5 = 1.5.  */
  check (dis (d1 + d2, 0x400E0000, 0), "double add");
  check (dis (d1 - d2, 0xBFE80000, 0), "double subtract");
  check (dis (d1 * d2, 0x400B0000, 0), "double multiply");
  check (dis (d2 / d1, 0x3FF80000, 0), "double divide");
  check (dis (dm * dm, 0x40220000, 0), "double multiply negatives");

  /* Comparisons, both ways and equal.  */
  check (d1 < d2 && !(d2 < d1) && !(d1 < d1), "double <");
  check (d1 <= d2 && d1 <= d1 && !(d2 <= d1), "double <=");
  check (d2 > d1 && !(d1 > d2) && !(d1 > d1), "double >");
  check (d2 >= d1 && d1 >= d1 && !(d1 >= d2), "double >=");
  check (d1 == d1 && !(d1 == d2) && d1 != d2 && !(d1 != d1), "double == !=");
  check (dm < dz && dz > dm, "double negative < 0");

  /* Conversions.  */
  check (dis ((double) lv, 0xC01C0000, 0), "long to double");
  check (dis ((double) uv, 0x41E65A0B, 0xC0000000), "unsigned long to double");
  check ((long) dm == -3 && (long) d2 == 2, "double to long");
  check ((unsigned long) dbig == 3000000000UL, "double to unsigned long");

  /* Floats.  */
  check (fis (f1 + f2, 0x40700000), "float add");
  check (fis (f1 - f2, 0xBF400000), "float subtract");
  check (fis (f1 * f2, 0x40580000), "float multiply");
  check (fis (f2 / f1, 0x3FC00000), "float divide");
  check (f1 < f2 && !(f2 < f1) && f1 <= f1 && f2 > f1 && f2 >= f2
	 && f1 == f1 && f1 != f2 && fm < f1, "float comparisons");
  check (fis ((float) lv, 0xC0E00000), "long to float");
  check (fis ((float) uv, 0x4F32D05E), "unsigned long to float");
  check ((long) fm == -3 && (long) f2 == 2, "float to long");
  check (dis ((double) f2, 0x40020000, 0), "float to double");
  check (fis ((float) d2, 0x40100000), "double to float");

  /* NaNs.  isunordered is exact everywhere (libgcc's __unord<mode>2).
     The compares are IEEE's with libgcc: unordered, every comparison but
     != false.  With -mos9math they're the math module's (T$DCmp, T$FCmp),
     as in Microware C: it knows no NaNs and compares sign and magnitude,
     so a positive NaN is larger than 1.5 and equal to itself.  Not with
     -ffinite-math-only, where GCC may fold the NaN tests away.  */
#if !__FINITE_MATH_ONLY__
  nan = __builtin_nan ("");
  fnan = __builtin_nanf ("");
  check (__builtin_isunordered (nan, d1) && !__builtin_isunordered (d1, d2)
	 && __builtin_isunordered (fnan, f1) && !__builtin_isunordered (f1, f2),
	 "isunordered");
#if __OS9MATH__
  check (nan > d1 && !(nan < d1) && nan == nan && !(nan != nan),
	 "double NaN, math module compare");
  check (fnan > f1 && !(fnan < f1) && fnan == fnan, "float NaN, math module compare");
#else
  check (!(nan < d1) && !(nan <= d1) && !(nan > d1) && !(nan >= d1)
	 && !(nan == nan) && nan != nan, "double NaN");
  check (!(fnan < f1) && !(fnan > f1) && !(fnan == fnan) && fnan != fnan,
	 "float NaN");
#endif
#else
  (void) nan;
  (void) fnan;
#endif

  printf ("%d failures\n", fails);
  return fails;
}
