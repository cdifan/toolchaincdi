/* Level 3 (partial): GCC -mos9call code calling Microware C 3.2's C
   library (clib.l and cstart.r, converted with rof2elf), and called back
   by it.  The harness has no OS-9, so only functions that make no system
   calls are used: not malloc, file I/O, or floating point (atof and
   printf's %f go through OS-9's math trap handler, trap #15).  Doubles
   are still passed, and their placement checked with %x.
   Compiled with -mos9call -mbuiltin=os9call; main returns a bit mask of
   the failed checks.

   Built with -DREAL_OS9 for OS-9 itself, it also checks what needs system
   calls: malloc, file I/O and floating point.  The mask, at most 16 bits,
   is the program's exit status there.  Either way the mask goes through
   exit(), since Microware's cstart.r ignores what main returns; the
   harness has the F$Exit system call it makes.  Text lines end in '\r', which is
   OS-9's end of line in either compiler (GCC's '\n' is a line feed
   without -mos9newline).  */

/* Microware's clib is K&R C; these are its functions as it defines
   them (int and double arguments, not char or float).  */
extern int strlen (), atoi (), strcmp (), sprintf (), sscanf (), toupper ();
extern long atol ();
extern char *strcpy (), *strcat (), *index ();
extern void qsort ();
#ifdef REAL_OS9
/* FILE pointers as char pointers, without Microware's stdio.h.  */
extern char *malloc (), *fopen (), *fgets ();
extern void free ();
extern int fprintf (), fclose (), unlink ();
extern double atof ();
#endif
extern void exit ();

struct big { int a[40]; };

static unsigned long fails;
/* Checks are numbered from 1: the mask is the exit status on OS-9, whose
   shell doesn't report a status of 1.  */
static void check (int ok, int n) { if (!ok) fails |= 1UL << n; }

/* Called back by Microware's qsort, with the OS-9 convention.  */
static int cmp_int (int *a, int *b) { return *a - *b; }

static int
eq (const char *a, const char *b)
{
  return strcmp (a, b) == 0;
}

int
main (void)
{
  char buf[64], word[16];
  int i, v[6] = { 5, 3, 9, 1, 7, 2 };
  struct big x, y;

  check (strlen ("hello") == 5, 1);
  check (atoi ("1234") == 1234, 1);
  check (strcmp ("abc", "abd") < 0 && strcmp ("b", "a") > 0, 2);
  strcpy (buf, "copy");
  strcat (buf, "cat");
  check (eq (buf, "copycat"), 3);
  sprintf (buf, "%d", 42);
  check (eq (buf, "42"), 4);
  /* Variadic: the format in d1, the rest on the stack.  */
  sprintf (buf, "%d %s %x %c", -7, "str", 255, 'z');
  check (eq (buf, "-7 str ff z"), 5);
  check (sscanf ("12 ab", "%d %s", &i, word) == 2 && i == 12
	 && eq (word, "ab"), 6);
  /* A GCC function called back by Microware code.  */
  qsort (v, 6, sizeof (int), cmp_int);
  check (v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 5 && v[4] == 7
	 && v[5] == 9, 7);
  /* GCC's own memcpy call for a struct copy goes to clib's memcpy.  */
  for (i = 0; i < 40; i++)
    x.a[i] = i * 3;
  y = x;
  check (y.a[0] == 0 && y.a[39] == 117, 8);
  /* clib's character table, through a6.  */
  check (toupper ('a') == 'A' && toupper ('1') == '1', 9);
  strcpy (word, "hello");
  check (index (word, 'l') == word + 2, 10);
  check (atol ("-123456") == -123456L, 11);
  /* A double as an unnamed argument: 8 bytes on the stack, high half first.
     %x reads the halves, which avoids the math trap that %f needs.  */
  sprintf (buf, "%d %x %x %d", 7, 1.5, 9);
  check (eq (buf, "7 3ff80000 0 9"), 12);
#ifdef REAL_OS9
  {
    char *p = malloc (100), *q = malloc (2000), *fp;
    check (p != 0 && q != 0 && p != q, 13);
    if (p != 0 && q != 0)
      {
	strcpy (p, "heap");
	q[1999] = 'x';
	check (eq (p, "heap") && q[1999] == 'x', 13);
      }
    free (q);
    free (p);

    /* A file written and read back, with an OS-9 text line.  */
    fp = fopen ("clibtst.tmp", "w");
    check (fp != 0 && fprintf (fp, "%d %s\r", 99, "lines") > 0, 14);
    if (fp != 0)
      fclose (fp);
    fp = fopen ("clibtst.tmp", "r");
    check (fp != 0 && fgets (buf, sizeof buf, fp) == buf
	   && eq (buf, "99 lines\r"), 14);
    if (fp != 0)
      fclose (fp);
    unlink ("clibtst.tmp");

    /* A double returned in d0:d1, and printed through the math trap.  */
    sprintf (buf, "%.2f %.1f", atof ("2.5") * 2, -0.25);
    check (eq (buf, "5.00 -0.2") || eq (buf, "5.00 -0.3"), 15);
  }
#endif
  /* Microware's cstart.r ignores what main returns; exit makes the
     F$Exit system call, which the harness has too.  */
  exit (fails);
}
