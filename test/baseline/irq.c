/* Level 0 corpus: interrupt handlers and big frames, whose prologue and
   epilogue paths the -mos9call changes touch.  */

extern void use (char *);
extern volatile int flag;
extern double g (double);

void __attribute__ ((interrupt_handler))
irq (void)
{
  flag++;
}

void __attribute__ ((interrupt_handler))
irq_calls (void)
{
  char buf[16];
  use (buf);
}

void __attribute__ ((interrupt_handler))
irq_big (void)
{
  char buf[40000];
  use (buf);
}

int
big (int a, int b)
{
  char buf[40000];
  use (buf);
  return a + b;
}

double
big_fp (double x)
{
  char buf[40000];
  use (buf);
  return g (x) * x;
}

int
var_frame (int n)
{
  char buf[n];
  use (buf);
  return buf[0];
}
