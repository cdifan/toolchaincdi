OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
SECTIONS {
    . = 0x400000;
    .text : { *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(2); }
    /* l68's symbols for the module's start, name and end: elf2mod sets
       every reference to them, so these values are only placeholders. */
    PROVIDE (btext = ADDR(.text)); PROVIDE (_btext = ADDR(.text));
    PROVIDE (bname = ADDR(.text)); PROVIDE (_bname = ADDR(.text));
    PROVIDE (etext = ADDR(.text)); PROVIDE (_etext = ADDR(.text));
    . = -0x8000;
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(2); }
    .data : { *(.data .data.*) . = ALIGN(2); _jmptbl = .; _ejmptbl = .; }
    .remote.data : { *(.remote.data .remote.data.*) . = ALIGN(2); }
    .remote.bss (NOLOAD) : { *(.remote.bss .remote.bss.*) . = ALIGN(2); }
    end = .;
    PROVIDE (_enddata = .);
}
