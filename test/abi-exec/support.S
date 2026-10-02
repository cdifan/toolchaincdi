| Runtime support for abirun test images, in place of a C library:
| memcpy and memset with the stack convention, as newlib's m68k
| versions have (GCC calls them for block moves, also under -mos9call).

	.text

	.globl	memcpy			| void *memcpy (void *d, const void *s, size_t n)
memcpy:
	move.l	4(%sp),%d0
	move.l	%d0,%a0
	move.l	8(%sp),%a1
	move.l	12(%sp),%d1
	bra.s	2f
1:	move.b	(%a1)+,(%a0)+
2:	subq.l	#1,%d1
	bcc.s	1b
	rts

	.globl	memset			| void *memset (void *d, int c, size_t n)
memset:
	move.l	4(%sp),%d0
	move.l	%d0,%a0
	move.l	12(%sp),%d1
	bra.s	2f
1:	move.b	11(%sp),(%a0)+
2:	subq.l	#1,%d1
	bcc.s	1b
	rts
