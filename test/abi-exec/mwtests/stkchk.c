/* Level 3 (partial): -mos9stkchk code checked by Microware C 3.2's own
   _stkcheck (from cstart.r, converted with rof2elf).  _stkcheck keeps the
   lowest stack address seen in _stbot and fails below _mtop; starting
   with _stbot at the top, each check that goes deeper lowers it.  */

/* volatile: _stkcheck changes them, which GCC can't see.  */
extern volatile unsigned long _stbot, _mtop;
extern void exit ();

static unsigned long fails;
/* Checks are numbered from 1: the mask is the exit status on OS-9, whose
   shell doesn't report a status of 1.  */
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
  check (depth (3) == 6, 1);
  first = _stbot;
#ifdef REAL_OS9
  /* On OS-9 itself, a little below main's own frame.  */
  check (first < (unsigned long) &first
	 && (unsigned long) &first - first < 1024, 2);
#else
  /* The harness's stack is below 0xE00000.  */
  check (first < 0xE00000 && first > 0xD00000, 2);
#endif
  check (depth (10) == 55, 3);
  check (_stbot < first, 4);
  /* depth (10) goes 7 levels deeper, each at least 32 bytes plus the
     68-byte margin's worth of frame.  */
  check (first - _stbot >= 7 * 32, 5);
  /* Microware's cstart.r ignores what main returns; exit makes the
     F$Exit system call, which the harness has too.  */
  exit (fails);
}
