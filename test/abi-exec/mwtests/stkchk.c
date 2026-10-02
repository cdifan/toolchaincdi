/* Level 3 (partial): -mos9stkchk code checked by Microware C 3.2's own
   _stkcheck (from cstart.r, converted with rof2elf).  _stkcheck keeps the
   lowest stack address seen in _stbot and fails below _mtop; starting
   with _stbot at the top, each check that goes deeper lowers it.  */

/* volatile: _stkcheck changes them, which GCC can't see.  */
extern volatile unsigned long _stbot, _mtop;

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

static int
depth (int n)
{
  volatile char buf[32];
  buf[0] = n;
  return n ? depth (n - 1) + buf[0] : 0;
}

int
main (void)
{
  unsigned long first;

  _mtop = 0;
  _stbot = 0xFFFFFFFF;
  check (depth (3) == 6, 0);
  first = _stbot;
  /* The harness's stack is below 0xE00000.  */
  check (first < 0xE00000 && first > 0xD00000, 1);
  check (depth (10) == 55, 2);
  check (_stbot < first, 3);
  /* depth (10) goes 7 levels deeper, each at least 32 bytes plus the
     68-byte margin's worth of frame.  */
  check (first - _stbot >= 7 * 32, 4);
  return fails;
}
