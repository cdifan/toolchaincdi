| Level 2, return values: float in d0 and double in d0:d1 (Microware
| C 3.2), structs through a buffer whose address the caller passes in
| a0 (Ultra C 2.5; the callee preserves a0).

	.text

	.globl	mw_retf			| float mw_retf (void): 1.5f
mw_retf:
	move.l	#0x3FC00000,%d0
	rts

	.globl	mw_retd			| double mw_retd (void): 2.5
mw_retd:
	move.l	#0x40040000,%d0
	moveq	#0,%d1
	rts

	.globl	mw_rets			| struct { int a, b, c; } mw_rets (int x):
mw_rets:				| { x, x + 1, x + 2 }
	move.l	%d0,(%a0)
	addq.l	#1,%d0
	move.l	%d0,4(%a0)
	addq.l	#1,%d0
	move.l	%d0,8(%a0)
	move.l	#0xBAD0BAD0,%d0		| nothing is returned in d0
	rts

	.globl	mw_rets1		| struct { char c; } mw_rets1 (int x): { x }
mw_rets1:
	move.b	%d0,(%a0)
	move.l	#0xBAD0BAD0,%d0
	rts

| Callers, calling GCC functions the Ultra C 2.5 way for structs.

	.globl	call_g_rets		| g_rets (5) into a buffer; returns a+b+c,
call_g_rets:				| or -1 if a0 wasn't preserved
	movem.l	%a0/%a2,-(%sp)		| a0 too: callers keep values there
	lea	-12(%sp),%sp
	move.l	%sp,%a0
	move.l	%a0,%a2
	moveq	#5,%d0
	jsr	g_rets(%pc)
	cmp.l	%a0,%a2
	bne.s	1f
	move.l	(%sp),%d0
	add.l	4(%sp),%d0
	add.l	8(%sp),%d0
	bra.s	2f
1:	moveq	#-1,%d0
2:	lea	12(%sp),%sp
	movem.l	(%sp)+,%a0/%a2
	rts

	.globl	call_g_retf		| returns g_retf ()'s d0 as is
call_g_retf:
	jsr	g_retf(%pc)
	rts
