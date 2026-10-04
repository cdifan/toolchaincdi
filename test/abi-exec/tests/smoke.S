| stub_add (a, b): returns a + b, arguments on the stack (the smoke test
| is compiled without -mos9call).
	.text
	.globl	stub_add
stub_add:
	move.l	4(%sp),%d0
	add.l	8(%sp),%d0
	rts

| os9_putc (c): writes the character c to the standard output path with
| I$Write (abirun emulates it); c is the low byte of its stack argument.
	.globl	os9_putc
os9_putc:
	lea	7(%sp),%a0
	moveq	#1,%d0			| standard output
	moveq	#1,%d1			| one byte
	trap	#0
	.word	0x8A			| I$Write
	rts
