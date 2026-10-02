/* Level 0: variadic functions, without -mos9call.  The fork's va_list
   handling for -mos9call must leave them unchanged: GCC folds va_start
   only without a target hook for it (found by an independent review).  */

#include <stdarg.h>

int
first (int a, ...)
{
  va_list ap;
  int r;
  va_start (ap, a);
  r = va_arg (ap, int);
  va_end (ap);
  return r;
}

long
sum (int n, ...)
{
  va_list ap;
  long s = 0;
  va_start (ap, n);
  while (n-- > 0)
    s += va_arg (ap, long);
  va_end (ap);
  return s;
}

double
dsum (const char *fmt, ...)
{
  va_list ap, aq;
  double s = 0;
  va_start (ap, fmt);
  va_copy (aq, ap);
  for (; *fmt; fmt++)
    s += *fmt == 'd' ? va_arg (aq, double) : va_arg (aq, int);
  va_end (aq);
  va_end (ap);
  return s;
}
