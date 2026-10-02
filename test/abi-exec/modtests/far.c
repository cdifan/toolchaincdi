/* Modules (elf2mod output): calls and address loads beyond 32 KB go
   through elf2mod's _jmptbl.  far.S puts 40 KB of code before far1,
   far2 and farstk.  main returns a bit mask of the failed checks.  */

extern int far1 (int), far2 (int);
extern int __attribute__ ((stackcall)) farstk (int);
extern int use_ptr (int, int, int (*) (int));
int (*volatile fp) (int);

/* A single direct call: bsr.w with -mbsrw.  */
int call_far2 (int a) { return far2 (a) * 10; }

/* A stack-convention function may tail-call (bra.w with -mbsrw).  */
int __attribute__ ((stackcall)) tail (int a) { return farstk (a); }

static unsigned long fails;
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

int
main (void)
{
  check (far1 (5) == 6, 0);			/* bsr.w, or lea + jsr */
  fp = far1;					/* lea: the real address */
  check (fp (1) == 2, 1);
  check (fp == far1, 2);
  check (use_ptr (1, 2, far2) == 5, 3);		/* pea */
  check (tail (4) == 7, 4);
  check (call_far2 (1) == 30, 5);
  return fails;
}
