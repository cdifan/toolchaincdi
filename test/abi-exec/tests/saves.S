| Level 2, register preservation: call GCC -mos9call functions the way
| Microware C 3.2 code does and check that every register except d0
| (and d1 when it carries an argument or the return value) survives, a6
| and sp included.  Microware C 3.2 callers rely on d1 and d2-d7 surviving
| calls, Ultra C 2.5 callers on a0/a1 too, and the manual says every
| register but argument and return registers is preserved
| (testos9c-cc32.a and -ucc25.a, tests 35, 36, 40-47).

	.text

| long mwcall (fn, a, b, flags): calls fn (a, b) with a in d0 and b in
| d1, every other register holding a marker value.  flags bit 0: d1
| doesn't carry an argument or the return value, so it must survive.
| mwcall itself preserves a0 and a1, as an OS-9 convention function must:
| its GCC callers keep values there.
| Returns a mask of the registers that changed (bit 0 = d1, 1-6 =
| d2-d7, 7-12 = a0-a5, 13 = a6 or sp); fn's result is left in mw_result/mw_result2.
| Called from GCC -mos9call code: fn in d0, a in d1, b at 4(sp), flags
| at 8(sp).

	.globl	mwcall
mwcall:
	movem.l	%d2-%d7/%a0-%a5,-(%sp)	| 48 bytes: b at 52(sp), flags at 56(sp)
	move.l	%d0,mw_target(%a6)
	move.l	56(%sp),mw_flags(%a6)
	clr.b	mw_a6bad(%a6)
	move.l	%d1,%d0			| a
	move.l	52(%sp),%d1		| b
	move.l	#0xD2D2D2D2,%d2
	move.l	#0xD3D3D3D3,%d3
	move.l	#0xD4D4D4D4,%d4
	move.l	#0xD5D5D5D5,%d5
	move.l	#0xD6D6D6D6,%d6
	move.l	#0xD7D7D7D7,%d7
	move.l	#0xA0A0A0A0,%a0
	move.l	#0xA1A1A1A1,%a1
	move.l	#0xA2A2A2A2,%a2
	move.l	#0xA3A3A3A3,%a3
	move.l	#0xA4A4A4A4,%a4
	move.l	#0xA5A5A5A5,%a5
	btst	#0,mw_flags+3(%a6)
	beq.s	1f
	move.l	#0xD1D1D1D1,%d1
1:	move.l	%a6,-(%sp)		| a6 (and sp) must survive: checked below
	pea	2f(%pc)			| return address
	move.l	mw_target(%a6),-(%sp)
	rts				| "call" fn without using a register
2:	cmp.l	(%sp),%a6
	beq.s	5f
	move.l	(%sp),%a6		| restored, to record the results
	st	mw_a6bad(%a6)
5:	addq.l	#4,%sp
	move.l	%d0,mw_result(%a6)
	move.l	%d1,mw_result2(%a6)
	movem.l	%d1-%d7/%a0-%a5,mw_regs(%a6)
	lea	mw_regs(%a6),%a0
	lea	mw_expect(%pc),%a1
	moveq	#0,%d0			| mask
	moveq	#0,%d1			| bit number
	btst	#0,mw_flags+3(%a6)
	bne.s	3f
	addq.l	#4,%a0			| d1 isn't checked
	addq.l	#4,%a1
	moveq	#1,%d1
3:	cmpm.l	(%a0)+,(%a1)+
	beq.s	4f
	bset	%d1,%d0
4:	addq.l	#1,%d1
	cmp.l	#13,%d1
	bne.s	3b
	tst.b	mw_a6bad(%a6)
	beq.s	6f
	bset	#13,%d0
6:
	movem.l	(%sp)+,%d2-%d7/%a0-%a5
	rts

mw_expect:
	.long	0xD1D1D1D1,0xD2D2D2D2,0xD3D3D3D3,0xD4D4D4D4,0xD5D5D5D5
	.long	0xD6D6D6D6,0xD7D7D7D7,0xA0A0A0A0,0xA1A1A1A1,0xA2A2A2A2
	.long	0xA3A3A3A3,0xA4A4A4A4,0xA5A5A5A5

| void clobber (void), stack convention: destroys every register that
| the stack convention lets a callee destroy (d0, d1, a0, a1).
	.globl	clobber
clobber:
	move.l	#0xDEAD0000,%d0
	move.l	#0xDEAD0001,%d1
	move.l	#0xDEAD00A0,%a0
	move.l	#0xDEAD00A1,%a1
	rts

	.bss
	.globl	mw_result, mw_result2
mw_target:	.skip	4
mw_flags:	.skip	4
mw_result:	.skip	4
mw_result2:	.skip	4
mw_regs:	.skip	13*4
mw_a6bad:	.skip	2
