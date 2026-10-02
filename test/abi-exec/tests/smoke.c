/* Smoke test for the harness (compiled without -mos9call): output, data through a6, a call into an
   assembly stub, and the exit status.  */

extern int stub_add (int, int);
static void putc_io (int c) { *(volatile char *) 0xF00000 = c; }
static int counter = 40;
int zero;

int
main (void)
{
  const char *s = "smoke\n";
  while (*s)
    putc_io (*s++);
  counter += 2 + zero;
  return counter == 42 && stub_add (40, 2) == 42 ? 0 : 1;
}
