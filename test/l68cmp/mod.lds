OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
SECTIONS {
    . = 0x400000;
    .text : { *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(2); }
    . = -0x8000;
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) . = ALIGN(2); }
    .data : { *(.data .data.*) . = ALIGN(2); _jmptbl = .; _ejmptbl = .; }
    .remote.data : { *(.remote.data .remote.data.*) . = ALIGN(2); }
    .remote.bss (NOLOAD) : { *(.remote.bss .remote.bss.*) . = ALIGN(2); }
    end = .;
}
