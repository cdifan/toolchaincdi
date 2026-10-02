| Far targets for modtests/far.c: 40 KB of code separate them from the
| C code.

	.text
	.globl	use_ptr
| int use_ptr (a, b, f): f (a + b); OS-9 convention (a in d0, b in d1,
| f on the stack).
use_ptr:
	add.l	%d1,%d0
	move.l	4(%sp),%a0
	jmp	(%a0)

	.space	40000

	.globl	far1, far2, farstk
far1:	addq.l	#1,%d0
	rts
far2:	addq.l	#2,%d0
	rts
| int farstk (int a): a + 3, stack convention
farstk:	move.l	4(%sp),%d0
	addq.l	#3,%d0
	rts
