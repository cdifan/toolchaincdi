| Startup code for abirun test images: call main, report its result.
| a6 and the stack pointer are set up by the harness.

	.text
	.globl	_start
_start:
	jsr	main(%pc)
	move.l	%d0,0xF00004		| IO_EXIT
1:	bra.s	1b

| The module header, for elf2mod and (through elf2rof) l68, which needs
| a root psect: a reentrant program module in object code, revision 1,
| edition 0, 3 KB of stack (elf2mod's defaults), entered at _start.
	.globl	__os9_tylan, __os9_attrev, __os9_edition, __os9_stack
	.globl	__os9_entry
	.set	__os9_tylan, 0x0101
	.set	__os9_attrev, 0x8001
	.set	__os9_edition, 0
	.set	__os9_stack, 0xC00
	.set	__os9_entry, _start
