| Startup code for abirun test images: call main, report its result.
| a6 and the stack pointer are set up by the harness.

	.text
	.globl	_start
_start:
	jsr	main(%pc)
	move.l	%d0,0xF00004		| IO_EXIT
1:	bra.s	1b
