| elf2mod resolves the symbols Microware's linker defines for the module's
| start (btext), name (bname) and end (etext): references to them near
| (PC16), far (beyond 32K: through the jump table) and as pointers in the
| data.  lsymcheck checks where each one arrives (make check-lsym).
| Assembled with --defsym BIGDATA=1, the module gets 40 KB of initialized
| data after the code, so that even the etext reference at the end of the
| code is too far for 16 bits.

	.text
	.globl	_start
_start:
	lea	btext(%pc),%a0		| +0: near
	lea	bname(%pc),%a1		| +4: near
	lea	etext(%pc),%a2		| +8: far (the module is larger than 32K)
	lea	_btext(%pc),%a3		| +12: Ultra C's name, near
	.space	40000
far:
	lea	btext(%pc),%a0		| +40016: far
	lea	bname(%pc),%a1		| +40020: far
	lea	etext(%pc),%a2		| +40024: near (far with BIGDATA)
	rts

	.data
	.long	btext, bname, etext, _bname
.ifdef BIGDATA
	.space	40000
.endif
