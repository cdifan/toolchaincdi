/* Level 3: -mos9newline, \n as OS-9's end of line (13) and \l as a line
   feed (10), as Microware C has them, with Microware's C library.  On OS-9
   itself (-DREAL_OS9), lines written with \n are read back by Microware's
   fgets, which ends a line at 13.  The mask of failed checks is the exit
   status, from bit 1 on (the OS-9 shell doesn't report a status of 1).  */

extern int sprintf (), strcmp ();
extern void exit ();
#ifdef REAL_OS9
extern char *fopen (), *fgets ();
extern int fprintf (), fclose (), unlink ();
#endif

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  char buf[32];

  check ('\n' == 13 && '\r' == 13 && '\l' == 10, 1);

  /* The bytes Microware's sprintf writes for a GCC format.  */
  sprintf (buf, "a\nb\lc%d\n", 7);
  check (buf[0] == 'a' && buf[1] == 13 && buf[2] == 'b' && buf[3] == 10
	 && buf[4] == 'c' && buf[5] == '7' && buf[6] == 13 && buf[7] == 0, 2);

#ifdef REAL_OS9
  {
    char *fp = fopen ("nltst.tmp", "w");

    check (fp != 0 && fprintf (fp, "one\ntwo\n") > 0, 3);
    if (fp != 0)
      fclose (fp);
    fp = fopen ("nltst.tmp", "r");
    check (fp != 0
	   && fgets (buf, sizeof buf, fp) == buf && strcmp (buf, "one\n") == 0
	   && fgets (buf, sizeof buf, fp) == buf && strcmp (buf, "two\n") == 0
	   && fgets (buf, sizeof buf, fp) == 0, 4);
    if (fp != 0)
      fclose (fp);
    unlink ("nltst.tmp");
  }
#endif

  exit (fails);
}
