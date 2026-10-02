| Stubs for keep.c.

	.text

| os9_inc (x): OS-9 convention, x in d0, returns x + 1 in d0; preserves
| every other register, a0 and a1 included.  It records a0 and a1, so
| the test can check that its caller kept something there.
	.globl	os9_inc
os9_inc:
	move.l	%a0,seen_a0(%a6)
	move.l	%a1,seen_a1(%a6)
	addq.l	#1,%d0
	rts

| stack_trash (x): stack convention, returns x + 1, and clobbers d1, a0
| and a1, as the stack convention allows.
	.globl	stack_trash
stack_trash:
	move.l	4(%sp),%d0
	addq.l	#1,%d0
	move.l	#0xdead0001,%d1
	move.l	#0xdead0002,%a0
	move.l	#0xdead0003,%a1
	rts

	.bss
	.globl	seen_a0, seen_a1
seen_a0:	.skip	4
seen_a1:	.skip	4
