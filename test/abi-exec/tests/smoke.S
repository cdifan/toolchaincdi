| stub_add (a, b): returns a + b, arguments on the stack (the smoke test
| is compiled without -mos9call).
	.text
	.globl	stub_add
stub_add:
	move.l	4(%sp),%d0
	add.l	8(%sp),%d0
	rts
