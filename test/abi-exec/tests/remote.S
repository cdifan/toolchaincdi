| char *peek (char *p, int i): p + i, computed outside GCC's view.
	.text
	.globl	peek
peek:
	add.l	%d1,%d0
	rts
