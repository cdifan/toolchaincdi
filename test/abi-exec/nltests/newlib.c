/* newlib, built for each multilib (test/abi-exec, make check-newlib):
   compiled with and without -mos9call, linked with the libc of that
   multilib.  Library calls both ways (qsort calls back), va_list passed
   to newlib (vsnprintf), setjmp/longjmp (assembly, in both conventions),
   with the registers longjmp restores, and malloc (through _sbrk,
   defined here).  main returns a bit mask of the failed checks.

   With -DREAL_OS9 (make check-os9newlib), for OS-9 itself: linked with
   -specs=os9.specs, whose libos9 has the system calls, and checking
   output, a file written and read back, and the time as well.  */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#ifdef REAL_OS9
#include <time.h>
#include <unistd.h>
#endif

#ifndef REAL_OS9
/* newlib is built without system calls; programs define them (here
   compiled with the same convention as the multilib that calls them).
   Only malloc's _sbrk does real work.  */
struct stat;

void
_exit (int status)
{
  *(volatile int *) 0xF00004 = status;	/* abirun's IO_EXIT */
  for (;;)
    ;
}

int _write (int fd, const void *buf, int n) { return n; }
int _read (int fd, void *buf, int n) { return 0; }
int _close (int fd) { return -1; }
long _lseek (int fd, long off, int whence) { return -1; }
int _fstat (int fd, struct stat *st) { return -1; }
int _isatty (int fd) { return 0; }
int _kill (int pid, int sig) { return -1; }
int _getpid (void) { return 1; }

static char heap[16384];
static size_t heap_used;

void *
_sbrk (ptrdiff_t incr)
{
  void *p = heap + heap_used;
  if (incr < 0 || heap_used + incr > sizeof heap)
    return (void *) -1;
  heap_used += incr;
  return p;
}
#endif

static int
cmp_int (const void *a, const void *b)
{
  int x = *(const int *) a, y = *(const int *) b;
  return x < y ? -1 : x > y;
}

/* The caller's variadic function hands its va_list to newlib.  */
static int
format (char *buf, size_t n, const char *fmt, ...)
{
  va_list ap;
  int r;
  va_start (ap, fmt);
  r = vsnprintf (buf, n, fmt, ap);
  va_end (ap);
  return r;
}

static jmp_buf env;

static void __attribute__ ((noinline))
jump (int v)
{
  longjmp (env, v);
}

/* int regs_after_longjmp (void): d2-d7 and a2-a5 hold marker values when
   setjmp is called, garbage when longjmp is; after it they must hold the
   markers again.  Returns a mask of those that don't (bits 2-7: d2-d7,
   10-13: a2-a5).  It preserves every register but d0, as both
   conventions allow; setjmp and longjmp are called in the multilib's
   convention, the jmp_buf on the stack.  */
extern int regs_after_longjmp (void);
__asm__ (
"	.pushsection .text\n"
"	.type	regs_after_longjmp, @function\n"
"regs_after_longjmp:\n"
"	movem.l	%d1-%d7/%a0-%a5,-(%sp)\n"
"	lea	-256(%sp),%sp\n"
"	move.l	#0xD2D2D2D2,%d2\n"
"	move.l	#0xD3D3D3D3,%d3\n"
"	move.l	#0xD4D4D4D4,%d4\n"
"	move.l	#0xD5D5D5D5,%d5\n"
"	move.l	#0xD6D6D6D6,%d6\n"
"	move.l	#0xD7D7D7D7,%d7\n"
"	move.l	#0xA2A2A2A2,%a2\n"
"	move.l	#0xA3A3A3A3,%a3\n"
"	move.l	#0xA4A4A4A4,%a4\n"
"	move.l	#0xA5A5A5A5,%a5\n"
#ifdef __OS9CALL__
"	move.l	%sp,%d0\n"
"	bsr.w	setjmp\n"
#else
/* Not move.l %sp,-(%sp): the 68000 pushes sp as it was before the
   decrement, CD-i Emulator's 68070 the decremented value.  */
"	move.l	%sp,%a0\n"
"	move.l	%a0,-(%sp)\n"
"	bsr.w	setjmp\n"
"	addq.l	#4,%sp\n"
#endif
"	tst.l	%d0\n"
"	bne.s	1f\n"
"	moveq	#-1,%d2\n"
"	moveq	#-1,%d3\n"
"	moveq	#-1,%d4\n"
"	moveq	#-1,%d5\n"
"	moveq	#-1,%d6\n"
"	moveq	#-1,%d7\n"
"	sub.l	%a2,%a2\n"
"	sub.l	%a3,%a3\n"
"	sub.l	%a4,%a4\n"
"	sub.l	%a5,%a5\n"
#ifdef __OS9CALL__
"	move.l	%sp,%d0\n"
"	moveq	#1,%d1\n"
"	bsr.w	longjmp\n"
#else
"	move.l	%sp,%a0\n"
"	pea	1.w\n"
"	move.l	%a0,-(%sp)\n"
"	bsr.w	longjmp\n"
#endif
"1:	moveq	#0,%d0\n"
"	cmp.l	#0xD2D2D2D2,%d2\n"
"	beq.s	2f\n"
"	bset	#2,%d0\n"
"2:	cmp.l	#0xD3D3D3D3,%d3\n"
"	beq.s	2f\n"
"	bset	#3,%d0\n"
"2:	cmp.l	#0xD4D4D4D4,%d4\n"
"	beq.s	2f\n"
"	bset	#4,%d0\n"
"2:	cmp.l	#0xD5D5D5D5,%d5\n"
"	beq.s	2f\n"
"	bset	#5,%d0\n"
"2:	cmp.l	#0xD6D6D6D6,%d6\n"
"	beq.s	2f\n"
"	bset	#6,%d0\n"
"2:	cmp.l	#0xD7D7D7D7,%d7\n"
"	beq.s	2f\n"
"	bset	#7,%d0\n"
"2:	cmp.l	#0xA2A2A2A2,%a2\n"
"	beq.s	2f\n"
"	bset	#10,%d0\n"
"2:	cmp.l	#0xA3A3A3A3,%a3\n"
"	beq.s	2f\n"
"	bset	#11,%d0\n"
"2:	cmp.l	#0xA4A4A4A4,%a4\n"
"	beq.s	2f\n"
"	bset	#12,%d0\n"
"2:	cmp.l	#0xA5A5A5A5,%a5\n"
"	beq.s	2f\n"
"	bset	#13,%d0\n"
"2:	lea	256(%sp),%sp\n"
"	movem.l	(%sp)+,%d1-%d7/%a0-%a5\n"
"	rts\n"
"	.size	regs_after_longjmp, . - regs_after_longjmp\n"
"	.popsection\n");

int
main (void)
{
  int fail = 0;
  char buf[64];
  int a[6] = { 42, -7, 1000, 3, 3, -500 };
  int i, n1, n2;
  char word[16];
  volatile int count = 0;
  char *p, *q;

  /* sprintf: integers, strings, characters, padding.  */
  sprintf (buf, "%d|%5s|%-4x|%c|%ld", -123, "ab", 255, 'z', 70000L);
  if (strcmp (buf, "-123|   ab|ff  |z|70000"))
    fail |= 1;

  /* sscanf and strtol.  */
  if (sscanf ("17 word -9", "%d %15s %d", &n1, word, &n2) != 3
      || n1 != 17 || strcmp (word, "word") || n2 != -9)
    fail |= 2;
  if (strtol ("-0x7fff", &p, 16) != -0x7fff || *p)
    fail |= 4;

  /* qsort calls the comparator back.  */
  qsort (a, 6, sizeof (int), cmp_int);
  for (i = 1; i < 6; i++)
    if (a[i - 1] > a[i])
      fail |= 8;

  /* A va_list passed on to newlib.  */
  if (format (buf, sizeof buf, "%s-%d-%s", "x", 99, "yz") != 7
      || strcmp (buf, "x-99-yz"))
    fail |= 16;

  /* setjmp/longjmp, including a value of 0 (returns 1).  */
  i = setjmp (env);
  count++;
  if (count == 1)
    jump (5);
  else if (count == 2)
    {
      if (i != 5)
	fail |= 32;
      jump (0);
    }
  else if (count != 3 || i != 1)
    fail |= 32;

  /* The registers setjmp saves, restored by longjmp.  */
  if (regs_after_longjmp () != 0)
    fail |= 1024;

  /* malloc and free, strcpy and strlen.  */
  p = malloc (100);
  q = malloc (2000);
  if (!p || !q || p == q)
    fail |= 64;
  else
    {
      strcpy (p, "heap");
      memset (q, 'x', 1999);
      q[1999] = '\0';
      if (strlen (p) != 4 || strlen (q) != 1999)
	fail |= 64;
      free (p);
      free (q);
    }

#ifdef REAL_OS9
  /* Output, a file written and read back (line ends unchanged), and the
     time.  */
  if (printf ("newlib on OS-9\n") != 15 || fflush (stdout))
    fail |= 128;
  {
    FILE *f = fopen ("nltest.tmp", "w");
    if (!f || fprintf (f, "one\ntwo\n") != 8 || fclose (f))
      fail |= 256;
    f = fopen ("nltest.tmp", "r");
    if (!f || !fgets (buf, sizeof buf, f) || strcmp (buf, "one\n")
	|| !fgets (buf, sizeof buf, f) || strcmp (buf, "two\n")
	|| fgets (buf, sizeof buf, f))
      fail |= 256;
    if (f)
      fclose (f);
    if (unlink ("nltest.tmp") || fopen ("nltest.tmp", "r"))
      fail |= 256;
  }
  if (time (NULL) < 24L * 3600 * 365 * 30)
    fail |= 512;
#endif

  return fail;
}
