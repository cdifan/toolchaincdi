/* Linker script for abirun test images, laid out like os9.lds in the
   README: code from 0x1000, data and bss from -0x8000 (a6-relative).  */
OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

SECTIONS {
    . = 0x1000;
    .text : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(2);
    }

    . = -0x8000;
    .data : {
        *(.data .data.*)
        . = ALIGN(2);
    }
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(2);
    }
}
