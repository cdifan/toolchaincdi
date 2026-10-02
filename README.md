# toolchaincdi

Dockerfile for developing CD-i (or OS-9/68000) applications.

This is a fork of [murachue/toolchaincdi](https://github.com/murachue/toolchaincdi), with
everything Murachue built kept as-is. It adds work on interoperating with Microware OS-9 C
compilers and libraries:
- an `-mos9call` option for the Microware C calling convention
- an opt-in `-mos9stkchk` option for Microware C 3.2-compatible stack checking
- `rof2elf` and `elf2rof` converters for Microware ROF object files

See [OS9-COMPAT-DESIGN.md](OS9-COMPAT-DESIGN.md) for the design and current status.
[`test/os9c/`](test/os9c/) holds two Microware C probes, for the calling convention and for stack
checking.

On the `compat-dev` branch, three submodules come from forks under
[github.com/cdifan](https://github.com/cdifan), each on a `-compat` branch next to Murachue's
unchanged one:
- GCC from [cdifan/gcc](https://github.com/cdifan/gcc) (`11.1.0-os9-compat`; `11.1.0-os9` is
  Murachue's work)
- newlib from [cdifan/newlib-cygwin](https://github.com/cdifan/newlib-cygwin)
  (`newlib-4.1.0-os9-compat`: `setjmp`/`longjmp` for `-mos9call`)
- elf2mod from [cdifan/elf2mod](https://github.com/cdifan/elf2mod) (`main-compat`, which adds
  `rof2elf` and `elf2rof`)

binutils and psximager still point at Murachue's repositories. `main` keeps Murachue's
toolchain until `compat-dev` is merged.

# Ingredients

- binutils (`m68k-elfos9-*`)
    - _no_ linker script for elf2mod, you must prepare that
    - `m68k-elfos9-as` defaults to a 68020-class CPU; assemble hand-written code with `-m68000 --pcrel`, so it rejects instructions the 68000/68070 lacks and keeps branches PC-relative
- gcc with OS-9/68000 customization (`m68k-elfos9-gcc`)
    - `-ma6rel` (with `-mpcrel`) is required for OS-9
    - `-mbsrw` may be shrink your app a bit
    - _no_ startup routine for OS-9, you must prepare that
- newlib
    - _no_ any syscall implementation but namespace clean
        - you must implement OS-9 syscall if you want, with `_` prefix
    - built twice (multilibs): for GCC's own calling convention and for `-mos9call`; the driver picks the right `libc.a` from the flags
- elf2mod
    - converts specially-crafted ELF into OS-9/68000 executable (module) file
    - [see elf2mod.md in the elf2mod fork](https://github.com/cdifan/elf2mod/blob/main-compat/elf2mod.md)
- rof2elf
    - converts Microware ROF objects (`.r`) and libraries (`.l`) into ELF, for linking with Microware's libraries
    - [see rof2elf.md in the elf2mod fork](https://github.com/cdifan/elf2mod/blob/main-compat/rof2elf.md)
- elf2rof
    - converts ELF objects and archives into ROF objects (`.r`) and libraries (`.l`), for linking GCC code with Microware's linker `l68`
    - [see elf2rof.md in the elf2mod fork](https://github.com/cdifan/elf2mod/blob/main-compat/elf2rof.md)
- psximager
    - psxbuild with CD-BRIDGE is your friend
    - no ability to make native CD-i image

# How to use?

An example, some application code in C, some Ruby code for generating data, build with GNU Make, follows:

```
$ cat <<'EOF' | sed 's/^    /\t/' > Makefile
all: build.img

build.img: build.cat build/CDI_TEST.APP build/SOMEDATA.RTF build/ABSTRACT.TXT build/COPYRGHT.TXT build/BIBLIOGR.TXT
    psxbuild --cuefile build.cat build.img

build/SOMEDATA.RTF: somedata.txt
    ruby mkdata.rb $< $@

build/CDI_TEST.APP: cdi_test.elf
    elf2mod $< $@

cdi_test.elf: os9.lds cstart.o main.o
    m68k-elfos9-gcc -nostdlib -Wl,-q -o $@ -T $+ -lc -lgcc

cstart.o: cstart.s
    m68k-elfos9-as -m68000 --pcrel -o $@ $<

main.o: main.c
    m68k-elfos9-gcc -c -mpcrel -ma6rel -Os -o $@ $<
EOF
$ cat <<EOF > cstart.s
    .type start, function
    .global start
start:
...
    bsr.w main
...
EOF
$ cat <<EOF > main.c
...
void main(void) {
    // some code...
    // sometimes with inline asm...
}
...
EOF
$ cat <<EOF > os9.lds
OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
EXTERN(start)
ENTRY(start)

SECTIONS {
    .text : {
        *(.text_startup)
        /* one statement, so each object's constants follow its code and stay
           within PC-relative reach in programs over 32K (newlib's stdio) */
        *(.text .text.* .rodata .rodata.*)
        /* *(.init)
            *(.fini) */
        . = ALIGN(2);
    }

    . = -0x8000; /* 32K only */

    /* OS-9/68000 linkers often place .data after .bss, but we follow other standard. */
    .data : {
        *(.data .data.*)
        . = ALIGN(2);
    }

    .bss(NOLOAD) : {
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(2);
    }

    end = .;
}
EOF
$ cat <<EOF > mkdata.rb
...
EOF
$ cat <<EOF > somedata.txt
...
EOF
$ mkdir build
$ cat <<EOF > build/ABSTRACT.TXT
this is a test.
EOF
$ cat <<EOF > build/COPYRGHT.TXT
copyright (c) 20xx
public domain
EOF
$ cat <<EOF > build/BIBLIOGR.TXT
murachue/toolchaincdi, 2021
EOF
$ cat <<EOF > build.cat
volume {
    system_id [CD-RTOS CD-BRIDGE]
    volume_id [TEST]
    volume_set_id [TEST]
    publisher_id [YOURNAME]
    preparer_id [YOURNAME]
    application_id [CDI_TEST.APP;1]
    copyright_file_id [COPYRGHT.TXT;1]
    abstract_file_id [ABSTRACT.TXT;1]
    bibliographic_file_id [BIBLIOGR.TXT;1]
}

dir {
    file CDI_TEST.APP
    file COPYRGHT.TXT
    file ABSTRACT.TXT
    file BIBLIOGR.TXT
    xafile SOMEDATA.RTF
}
EOF
$ cat <<EOF > Dockerfile
FROM ghcr.io/cdifan/toolchaincdi
RUN apt update && apt install --no-install-recommends -y make ruby && apt clean
EOF
$ docker run --rm -ti -v $PWD:/work -w /work -u $(id -u):$(id -g) $(docker build -q .) make
```

You'll get `build.bin` and `build.cat`.

Note that if you run docker without `-u`, built files are owned by root. Be careful.

psxbuild (libcdio) will says something about failing filename constraints, but it's OK.

The image is built by GitHub Actions on every push to `main` and published as
`ghcr.io/cdifan/toolchaincdi`. The original `ghcr.io/murachue/toolchaincdi` image predates this
fork's changes.

# License

MIT for Dockerfile.

The submodules keep their own licenses. The GCC and newlib changes are distributed under
the licenses of the files they modify (GCC: GPL version 3 or later). See section 10 of
[OS9-COMPAT-DESIGN.md](OS9-COMPAT-DESIGN.md) for authorship and upstreaming notes.
