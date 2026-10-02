/* Linker script for abirun test images, laid out like os9.lds in the
   README, except that .bss comes first: bss and data from -0x8000
   (a6-relative), then remote data beyond the 64 KB that a6 reaches.  With
   .bss first, .data and .remote.data are adjacent, so elf2mod stores no
   zeros for .bss in the module's initialized data.  Code is placed above
   the data area (elf2mod and the harness accept any code address).  With
   more than 32 KB of data, sections cross from 0xFFFFFFFF to 0: link with
   --no-check-sections, which allows that.  */
OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

SECTIONS {
    . = 0x400000;
    .text : {
        /* One statement, so each object's constants follow its code:
           code reaches them PC-relative (+-32 KB), also in programs with
           more code than that (newlib's stdio).  */
        *(.text .text.* .rodata .rodata.*)
        . = ALIGN(2);
    }

    . = -0x8000;
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(2);
    }
    .data : {
        *(.data .data.*)
        . = ALIGN(2);
        /* elf2mod's jump table for far calls: --defsym __jmptbl_size=N */
        _jmptbl = .;
        . += DEFINED(__jmptbl_size) ? __jmptbl_size : 0;
        _ejmptbl = .;
    }
    .remote.data : {
        *(.remote.data .remote.data.*)
        . = ALIGN(2);
    }
    .remote.bss (NOLOAD) : {
        *(.remote.bss .remote.bss.*)
        . = ALIGN(2);
    }
    end = .;
}
