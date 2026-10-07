/* lsym.S's linker script: as m68k-os9-xcc's, with a jump table
   (--defsym __jmptbl_size=N) and the symbols Microware's linker defines,
   which elf2mod resolves.  */
OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
PROVIDE (__jmptbl_size = 0);
SECTIONS {
    . = 0x400000;
    .text : { *(.text .text.* .rodata .rodata.*) . = ALIGN(2); }
    PROVIDE (btext = ADDR(.text)); PROVIDE (_btext = ADDR(.text));
    PROVIDE (bname = ADDR(.text)); PROVIDE (_bname = ADDR(.text));
    PROVIDE (etext = ADDR(.text)); PROVIDE (_etext = ADDR(.text));
    . = -0x8000;
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(2); }
    .data : { *(.data .data.*) . = ALIGN(2); _jmptbl = .; . += __jmptbl_size; _ejmptbl = .; }
    .remote.data : { *(.remote.data .remote.data.*) . = ALIGN(2); }
    .remote.bss (NOLOAD) : { *(.remote.bss .remote.bss.*) . = ALIGN(2); }
    end = .;
    PROVIDE (_enddata = .);
}
