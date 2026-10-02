/* stkchk.c - probe of Microware C 3.2 stack checking (compile without -s) */

extern int ext();
int gx;

/* leaf function, no locals */
int leaf(i)
int i;
{
	return i + 1;
}

/* function with a large local array (big frame) */
int bigframe(i)
int i;
{
	int a[1000];
	a[i] = i;
	return ext(a, i);
}

/* non-leaf function with a small frame */
int nonleaf(i)
int i;
{
	int t;
	t = ext(i);
	return t + gx;
}

/* two arguments: is d1 (argument 2) reloaded after _stkcheck? */
int leaf2(i, j)
int i, j;
{
	return j - i;
}

/* two arguments, big frame, j used first */
int bigframe2(i, j)
int i, j;
{
	int a[1000];
	a[j] = i;
	return ext(a, j);
}

/* double argument in d0:d1 */
double leafd(d)
double d;
{
	return d + 1.0;
}
