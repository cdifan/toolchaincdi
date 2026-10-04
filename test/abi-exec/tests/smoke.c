/* Smoke test for the harness (compiled without -mos9call): output, data through a6, a call into an
   assembly stub, and the exit status.  Output goes through I$Write (os9_putc, in smoke.S), so the
   test also runs on OS-9.  */

extern int stub_add (int, int);
extern void os9_putc (int);
static int counter = 40;
int zero;

int
main (void)
{
  const char *s = "smoke\n";
  while (*s)
    os9_putc (*s++);
  counter += 2 + zero;
  return counter == 42 && stub_add (40, 2) == 42 ? 0 : 1;
}
