/* Level 2, libgcc's soft-float routines (lb1sf68.S): arithmetic, and the
   state they keep in _fpCCR.  With -mpcrel they're assembled as PIC; they
   reached _fpCCR through the GOT, which doesn't exist on OS-9, instead of
   relative to a6 like other data.  A division by zero must set the
   exception bits in the real _fpCCR.  main returns a bit mask of the
   failed checks.  */

/* volatile: GCC assumes its soft-float calls don't touch memory.  */
extern volatile struct
{
  short exception_bits, trap_enable_bits, sticky_bits, rounding_mode;
  short format, last_operation;
} _fpCCR;

static volatile double one = 1.0, three = 3.0, zero = 0.0, big = 1e300;
static volatile float fthree = 3.0f;

int
main (void)
{
  int fail = 0;
  volatile double d;
  volatile float f;
  volatile long l;

  d = one / three;
  if (d < 0.3333333333333333 || d > 0.3333333333333334)
    fail |= 1;
  d = three * three + one;
  if (d != 10.0)
    fail |= 2;
  f = fthree * fthree;
  if (f != 9.0f)
    fail |= 4;
  l = (long) (big / 1e298);
  if (l != 100)
    fail |= 8;

#ifndef __HAVE_68881__
  /* Soft-float only: the state in _fpCCR.  */
  if (_fpCCR.rounding_mode != 0)	/* ROUND_TO_NEAREST */
    fail |= 16;
  _fpCCR.exception_bits = 0;
  d = one / zero;
  if (_fpCCR.exception_bits == 0)
    fail |= 32;
#endif
  return fail;
}
