/* newlib, built for each multilib (test/abi-exec, make check-newlib):
   compiled with and without -mos9call, linked with the libc of that
   multilib.  Library calls both ways (qsort calls back), va_list passed
   to newlib (vsnprintf), setjmp/longjmp (assembly, in both conventions)
   and malloc (through _sbrk, defined here).  main returns a bit mask of
   the failed checks.  */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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

  return fail;
}
