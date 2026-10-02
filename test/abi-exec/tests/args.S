| Level 2, argument passing: Microware C 3.2-style assembly on the other
| side of GCC -mos9call code.  Microware C 3.2 rules (testos9c-cc32.a):
| argument 1 in d0 (a double in d0:d1), argument 2 in d1 if argument 1
| used d0 alone, the rest on the stack, pushed right to left; structs
| on the stack at the start of a 4-byte-rounded slot; the caller pops.

	.text

| Callees, as Microware C 3.2 code would receive the arguments.

	.globl	mw_sub3			| int mw_sub3 (a, b, c): a - b - c
mw_sub3:
	sub.l	%d1,%d0
	sub.l	4(%sp),%d0
	rts

	.globl	mw_dhi			| int mw_dhi (double x, int y): hi(x) + y
mw_dhi:
	add.l	4(%sp),%d0
	rts

	.globl	mw_dlo			| int mw_dlo (double x): lo(x)
mw_dlo:
	move.l	%d1,%d0
	rts

	.globl	mw_s1			| int mw_s1 (struct { char c; } s, int i)
mw_s1:
	moveq	#0,%d0
	move.b	4(%sp),%d0		| the struct is at the start of its slot
	add.l	8(%sp),%d0
	rts

	.globl	mw_s3			| int mw_s3 (struct { char a, b, c; } s, int i)
mw_s3:
	moveq	#0,%d0
	move.b	6(%sp),%d0		| s.c
	add.l	8(%sp),%d0
	rts

	.globl	mw_idi			| int mw_idi (int a, double b, int c)
mw_idi:
	add.l	4(%sp),%d0		| hi(b)
	add.l	12(%sp),%d0		| c
	rts

	.globl	mw_var			| int mw_var (fmt, ...) called with 3 ints
mw_var:
	sub.l	%d1,%d0
	sub.l	4(%sp),%d0
	rts

	.globl	mw_ll			| int mw_ll (long long x, int y): lo(x) + y
mw_ll:
	move.l	%d1,%d0
	add.l	4(%sp),%d0
	rts

| Callers, calling GCC functions the way Microware C 3.2 code does.

	.globl	call_g_sub3		| g_sub3 (100, 20, 3)
call_g_sub3:
	pea	3.w
	moveq	#20,%d1
	moveq	#100,%d0
	jsr	g_sub3(%pc)
	addq.l	#4,%sp
	rts

	.globl	call_g_dhi		| g_dhi (1.5, 5)
call_g_dhi:
	pea	5.w
	move.l	#0x3FF80000,%d0
	moveq	#0,%d1
	jsr	g_dhi(%pc)
	addq.l	#4,%sp
	rts

	.globl	call_g_s1		| g_s1 ((struct s1) { 7 }, 100)
call_g_s1:
	pea	100.w
	move.l	#0x07000000,-(%sp)	| byte 7 at the start of the slot
	jsr	g_s1(%pc)
	addq.l	#8,%sp
	rts

	.globl	call_g_idi		| g_idi (10, 1.5, 5)
call_g_idi:
	pea	5.w
	clr.l	-(%sp)
	move.l	#0x3FF80000,-(%sp)
	moveq	#10,%d0
	jsr	g_idi(%pc)
	lea	12(%sp),%sp
	rts
