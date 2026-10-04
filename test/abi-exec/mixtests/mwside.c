/* Level 3 on OS-9 itself, mixed: this side is compiled by Microware C 3.2
   (natively, cc -r), the other (gccside.c) by GCC with -mos9call, and the
   two are linked together, by Microware's l68 or by GNU ld.  Calls go both
   ways.  K&R C, as Microware C 3.2 takes it.  */

struct pair { int a; int b; };
struct big { int v[10]; };

/* Checks of the arguments that arrive from GCC code, one bit each.  */
int mwfails;

#define CHECK(ok, n) if (!(ok)) mwfails |= 1 << (n)

/* Called by GCC code: arguments.  Each returns a value the caller checks.  */

int m01(a, b)
int a, b;
{
	CHECK(a == 1 && b == 2, 1);
	return a + b * 10;
}

int m02(a, b, c)
int a, b, c;
{
	CHECK(a == 1 && b == 2 && c == 3, 2);
	return 2;
}

int m03(a, b, c, d, e)
int a, b, c, d, e;
{
	CHECK(a == 1 && b == 2 && c == 3 && d == 4 && e == 5, 3);
	return a + b + c + d + e;
}

int m04(a, x, b)
int a;
double x;
int b;
{
	CHECK(a == 1 && x == 2.5 && b == 3, 4);
	return 4;
}

int m05(x, a, b)
double x;
int a, b;
{
	CHECK(x == -0.5 && a == 1 && b == 2, 5);
	return 5;
}

int m06(x, y)
double x, y;
{
	CHECK(x == 1.25 && y == 1e10, 6);
	return 6;
}

int m07(a, x)
int a;
double x;
{
	CHECK(a == 7 && x == 3.0, 7);
	return 7;
}

int m08(p, q, r)
char *p, *q, *r;
{
	CHECK(p[0] == 'p' && q[0] == 'q' && r[0] == 'r', 8);
	return 8;
}

int m09(c, s, uc, us)
char c;
short s;
unsigned char uc;
unsigned short us;
{
	CHECK(c == -3 && s == -1000 && uc == 200 && us == 60000, 9);
	return 9;
}

int m10(f, a)
float f;
int a;
{
	CHECK(f == 0.75 && a == 10, 10);
	return 10;
}

int m11(p, a)
struct pair p;
int a;
{
	CHECK(p.a == 11 && p.b == 12 && a == 13, 11);
	return 11;
}

int m12(a, p, b)
int a;
struct pair p;
int b;
{
	CHECK(a == 1 && p.a == 2 && p.b == 3 && b == 4, 12);
	return 12;
}

int m13(g, a)
struct big g;
int a;
{
	CHECK(g.v[0] == 100 && g.v[9] == 109 && a == 14, 13);
	return 13;
}

int m14(a, b, p)
int a, b;
struct pair p;
{
	CHECK(a == 1 && b == 2 && p.a == 3 && p.b == 4, 14);
	return 14;
}

/* Called by GCC code: return values.  */

char m15()
{
	return -2;
}

short m16()
{
	return -30000;
}

unsigned char m17()
{
	return 250;
}

int *m18()
{
	static int v = 18;

	return &v;
}

float m19()
{
	return 1.5;
}

double m20()
{
	return -2.25;
}

long m21()
{
	return 123456789;
}

/* Calls of GCC code, through a pointer too; a bit for each failure.  */

extern int g01(), g03(), g04(), g09(), g10(), g11(), g12(), gsum();
extern char g15();
extern int *g18();
extern float g19();
extern double g20();

int mwcalls(fp)
int (*fp)();
{
	struct pair p;
	float fv;
	int fails = 0;

	p.a = 11;
	p.b = 12;

	if (g01(1, 2) != 21)
		fails |= 1 << 1;
	if (g03(1, 2, 3, 4, 5) != 15)
		fails |= 1 << 3;
	if (g04(1, 2.5, 3) != 4)
		fails |= 1 << 4;
	if (g09(-3, -1000, 200, 60000) != 9)
		fails |= 1 << 9;
	/* float variables, which C 3.2 passes as doubles */
	fv = 2.5;
	if (g10(1, fv, 3) != 10)
		fails |= 1 << 10;
	fv = -2.5;
	if (g12(fv, 7) != 12)
		fails |= 1 << 12;
	if (g11(p, 13) != 11)
		fails |= 1 << 11;
	if (g15() != -2)
		fails |= 1 << 15;
	if (*g18() != 18)
		fails |= 1 << 18;
	if (g19() != 1.5)
		fails |= 1 << 19;
	if (g20() != -2.25)
		fails |= 1 << 20;
	if (gsum(4, 10, 20, 30, 40) != 100)
		fails |= 1 << 22;
	if ((*fp)(1, 2) != 21)
		fails |= 1 << 23;

	return fails;
}
