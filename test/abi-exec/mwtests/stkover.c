/* Level 3, -mos9stkchk's overflow path, on OS-9 itself: recursion without
   end, each call checked by Microware C 3.2's _stkcheck (cstart.r), which
   must stop it when the stack reaches the program's data, write
   "**** Stack Overflow ****" on the error path and exit.  It means to exit
   with 0x101: it pushes 257 for _exit, but _exit takes its argument in d0,
   which still holds 2, the path number of its I$WritLn, so the status is
   2 (Error #000:002), as check-os9 expects (stkover.status).  A crash
   would end it with another (a bus error, 000:102).  Without REAL_OS9,
   in the Level 2 harness, it does nothing: there _stkcheck has no limit
   (its _mtop is set by cstart's own entry point, which the harness
   doesn't run).  */

extern void exit ();

/* The frame stays live across the call (the array escapes and is read
   after it), so the recursion can't become a loop.  */
static void __attribute__ ((noinline))
use (volatile char *p)
{
  p[0]++;
}

static int __attribute__ ((noinline))
deep (int n)
{
  volatile char buf[256];

  buf[0] = n;
  use (buf);
  return deep (n + 1) + buf[0];
}

int
main (void)
{
#ifdef REAL_OS9
  deep (0);
#endif
  exit (0);
  return 0;
}
