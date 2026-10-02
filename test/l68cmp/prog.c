/* Compared between l68 -g and rof2elf + ld + elf2mod -g (run.sh):
   code, initialized and uninitialized data, a static (not in the
   symbol module), and library functions.  K&R, for Microware C.  */

int counter;
int table[3] = {1, 2, 3};
static int hidden = 5;
char *msg = "hello";
int (*fp)() = 0;

int
add (a, b)
     int a, b;
{
  return a + b + hidden;
}

main ()
{
  fp = add;
  counter = (*fp) (table[1], 4);
  printf ("%s %d\n", msg, counter);
  exit (0);
}
