| Level 2, variadic functions: GCC -mos9call variadic functions called
| the way Microware C 3.2 code calls them (it has no prototypes, so a
| variadic call is placed like any K&R call): argument 1 in d0, argument
| 2 in d1 if argument 1 used d0 alone and argument 2 is an int or
| pointer, the rest on the stack, pushed right to left.

	.text

| sum (3, 10, 20, 30): n in d0, 10 in d1, 20 and 30 on the stack.
	.globl	call_sum
call_sum:
	pea	30.w
	pea	20.w
	moveq	#10,%d1
	moveq	#3,%d0
	jsr	sum(%pc)
	addq.l	#8,%sp
	rts

| mix ("idi", 7, 1.5, 9): fmt in d0, 7 in d1, 1.5 and 9 on the stack.
	.globl	call_mix_idi
call_mix_idi:
	pea	9.w
	clr.l	-(%sp)
	move.l	#0x3FF80000,-(%sp)
	moveq	#7,%d1
	lea	fmt_idi(%pc),%a0
	move.l	%a0,%d0
	jsr	mix(%pc)
	lea	12(%sp),%sp
	rts

| mix ("di", 1.5, 9): fmt in d0, the double on the stack, d1 unused.
	.globl	call_mix_di
call_mix_di:
	pea	9.w
	clr.l	-(%sp)
	move.l	#0x3FF80000,-(%sp)
	move.l	#0xBADBAD,%d1		| garbage: d1 holds no argument
	lea	fmt_di(%pc),%a0
	move.l	%a0,%d0
	jsr	mix(%pc)
	lea	12(%sp),%sp
	rts

| two (1, 2, 3, 4): a in d0, b in d1, 3 and 4 on the stack.
	.globl	call_two
call_two:
	pea	4.w
	pea	3.w
	moveq	#2,%d1
	moveq	#1,%d0
	jsr	two(%pc)
	addq.l	#8,%sp
	rts

| dfirst (1.5, 7, 8): the double in d0:d1, 7 and 8 on the stack.
	.globl	call_dfirst
call_dfirst:
	pea	8.w
	pea	7.w
	move.l	#0x3FF80000,%d0
	moveq	#0,%d1
	jsr	dfirst(%pc)
	addq.l	#8,%sp
	rts

fmt_idi:	.asciz	"idi"
fmt_di:		.asciz	"di"
	.even
