/* Level 2, remote data (the remote attribute): variables beyond the
   64 KB that a6-relative addressing reaches, addressed as a6 plus a
   32-bit offset.  40 KB of normal data (the a6 window holds up to 64 KB)
   and a 70,000-byte remote array reach well beyond the window.  main
   returns a bit mask of the failed checks.  */

volatile char filler[40000];				/* .bss, across 0 */
char big[70000] __attribute__ ((remote));		/* .remote.bss */
int rinit[4] __attribute__ ((remote)) = { 11, 22, 33, 44 };	/* .remote.data */
extern char *peek (char *, int);			/* remote.S */

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  int i;
  long sum = 0;
  int *rp;

  filler[0] = 1;
  filler[39999] = 2;
  for (i = 0; i < 70000; i += 1000)
    big[i] = i / 1000;
  for (i = 0; i < 70000; i += 1000)
    sum += big[i];
  check (sum == 69 * 70 / 2, 0);
  check (rinit[0] == 11 && rinit[1] == 22 && rinit[3] == 44, 1);
  rp = &rinit[2];
  check (*rp == 33, 2);
  big[69999] = 5;
  check (*peek (big, 69999) == 5, 3);
  check (&big[69999] - &big[0] == 69999, 4);
  /* The remote data comes after the normal data.  */
  check ((unsigned long) &big[0] - (unsigned long) filler >= 40000, 5);
  /* Normal data across 0 (-0x8000 + 40000), still a6-relative.  */
  check (filler[0] == 1 && filler[39999] == 2, 6);
  return fails;
}
