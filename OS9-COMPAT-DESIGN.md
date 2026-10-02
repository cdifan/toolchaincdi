# OS-9 compatibility design notes

*Written with assistance from Claude (Anthropic); research, analysis and text by Claude, design
decisions and review by CD-i Fan <cdifan@gmail.com>.*

Design notes and plans for extending this toolchain (`m68k-elfos9`, the GCC/binutils/newlib
fork used for CD-i and OS-9/68000 development) so that GCC-built code can interoperate with code
and libraries built by Microware's own compilers.

Status: **planning**. Apart from the Microware test probe in [`test/os9c/`](test/os9c/), nothing
described here has been implemented yet.

Contents:

1. [Current toolchain](#1-current-toolchain)
2. [The GCC fork vs. mainline](#2-the-gcc-fork-vs-mainline)
3. [Microware C calling convention](#3-microware-c-calling-convention)
4. [`-mos9call` design](#4--mos9call-design)
5. [How dependent is the toolchain on ELF?](#5-how-dependent-is-the-toolchain-on-elf)
6. [ROF interoperability: `rof2elf` and `elf2rof`](#6-rof-interoperability-rof2elf-and-elf2rof)
7. [Microware tools available for testing](#7-microware-tools-available-for-testing)
8. [Build environment](#8-build-environment)
9. [Emulator serial bridge](#9-emulator-serial-bridge)
10. [Licensing and upstreaming](#10-licensing-and-upstreaming)
11. [Test plan](#11-test-plan)
12. [Open questions and next steps](#12-open-questions-and-next-steps)
13. [References](#13-references)

---

## 1. Current toolchain

The [`Dockerfile`](Dockerfile) builds, from the pinned submodules under `src/`:

| Component | Source | Notes |
|---|---|---|
| binutils 2.35 | [murachue/binutils-gdb](https://github.com/murachue/binutils-gdb) `binutils-2_35-branch` | unmodified upstream 2.35; the target name is accepted through generic m68k patterns |
| GCC 11.1.0 | [cdifan/gcc](https://github.com/cdifan/gcc) `11.1.0-os9` (Murachue's branch, unchanged) | `-mpcrel`, `-ma6rel`, `-mbsrw` (see §2) |
| newlib 4.1.0 | [murachue/newlib-cygwin](https://github.com/murachue/newlib-cygwin) `newlib-4.1.0-os9` | no syscalls |
| elf2mod | [murachue/elf2mod](https://github.com/murachue/elf2mod) | linked ELF → OS-9 module |
| psximager | [murachue/psximager](https://github.com/murachue/psximager) `cdi` | disc images |

Build flow for an application:

```
gcc -mpcrel -ma6rel  →  as (ELF .o)  →  ld -q -T os9.lds (ELF, .data at -0x8000)  →  elf2mod  →  OS-9 module
```

The OS-9 data model: code is position independent (PC-relative), and global data is addressed
relative to `a6`, which points 0x8000 bytes past the start of the static storage area. a5 is the
frame pointer when one is used (code built with `-fomit-frame-pointer`, such as newlib, has none).

## 2. The GCC fork vs. mainline

The `11.1.0-os9` branch is exactly **9 commits on top of the `releases/gcc-11.1.0` tag**
(commit `50bc918`), 0 commits behind:

```
053e3cae1 static const struct[][] in a function was become a6rel but should be pcrel
c68a391d2 SYMBOL_REF_DECL for LABEL_REF is wrong, and LABEL_REF never points data so decl==NULL is ok
186b55267 HACK: use adjust_address_nv for XFmode to avoid assertion fail on compiling libgcc:mulxc3/divxc3
2f07cfbd6 don't a6rel on rodata that will be meld to .text
6a48ce2d1 a6rel-able rtx may be const-of-plus-of-sym-and-constint
79f1fa700 a5 is usable when -mpcrel
ce888142f target m68k-*-elfos9 for binutils
c77e9d47a -ma6rel uses %a6 for var_decls
4aa5c8071 -mbsrw use bsr.w for inter-function call
50bc9185c c++: Remove #error for release builds        <- releases/gcc-11.1.0
```

Total: 7 files changed, 89 insertions, 15 deletions (`gcc/config.gcc`, and in
`gcc/config/m68k/`: `m68k-os9.h`, `m68k.c`, `m68k.h`, `m68k.md`, `m68k.opt`, `predicates.md`).

To get this history into the shallow submodule:

```sh
git -C src/gcc fetch --depth 1 origin 50bc9185c2821350f0b785d6e23a6e9dcde58466
git -C src/gcc tag gcc-11.1.0-base 50bc9185c2821350f0b785d6e23a6e9dcde58466
git -C src/gcc fetch --depth=10 origin 053e3cae1ce30de60a7e1f91421a00cb41670849
git -C src/gcc diff --stat gcc-11.1.0-base HEAD
```

### Portability to current mainline (GCC 17)

The whole fork diff was applied as a dry run to gcc-mirror `master` (`BASE-VER` 17.0.0):

- After mapping `m68k.c` → `m68k.cc` (renamed in GCC 12), every hunk applies, only at shifted
  line numbers. That covers `-mbsrw`, `-ma6rel` (including the `sym(%a6)` output in
  `print_operand_address`), the a6 reservation in `m68k_conditional_register_usage`, and the
  `adjust_address_nv` hack.
- The one rejected hunk (`predicates.md`) only adds a comment.
- **Must fix:** `dbxelf.h` no longer exists (stabs support was removed in GCC 13), so it has to
  be dropped from the `m68k-*-elfos9*` `tm_file` line in `config.gcc`.
- **Watch:** m68k still defaults to the old register allocator ("reload") in master, with LRA
  behind the undocumented `-mlra`. If reload is removed, `-ma6rel`/`-mpcrel` need retesting
  under LRA.
- The cc0 removal in GCC 12 rewrote m68k condition-code handling, but none of the fork's commits
  touch that area.
- binutils needs no changes at all: the pinned commit is upstream 2.35, and the `elfos9` name is
  accepted through generic patterns.

None of this has been built or tested yet. The argument-passing hooks (`function_arg_info`) are
the same from GCC 10 through 17, so `-mos9call` (§4) would be nearly identical on either base.
**Decision:** implement it on GCC 11.1, the fork's current baseline. Porting to GCC 17 can follow
later, separately, possibly by someone else; the portability notes above are its starting point.

## 3. Microware C calling convention

### What the manual says

*OS-9 C Language User Manual* (Microware C compiler 3.1, 1991), chapter 3, "Compiler
Organization":

- **Registers:** d0–d1 for arguments and return values; d2–d3 and a0–a1 for compiler
  temporaries; d4–d7 and a2–a4 for register variables. a5 is the frame pointer, a6 the base of
  static storage (never changed by C code), a7 the stack pointer.
- **Arguments:**
  - The first integral argument (int, long, pointer, or char/short widened to int) goes in d0,
    the second in d1.
  - A double as the first argument goes in d0:d1, most significant half in d0.
  - Remaining arguments are pushed on the stack.
  - If the first argument is integral and the second a double, the double goes entirely on the
    stack.
- **Floats** are converted to double when passed.
- **Return values:** integral and float values in d0, doubles in d0:d1.
- **Preserved registers:** "All functions … are required to restore any changed registers … The
  only exceptions to this are function return register(s) and register(s) in which the
  function's argument(s) are passed."
- **Stack arguments** are 4-byte longwords. char/short are sign-extended, unsigned char/short
  zero-extended.
- Calls use `bsr`. The linker builds a jump table (`_jmptbl`) for targets beyond ±32K.

### What the compiler actually does

[`test/os9c/testos9c.c`](test/os9c/testos9c.c) is a K&R C probe covering the cases the manual
leaves open. Its header explains the test numbering. It has been compiled with two Microware
compilers:

| Output | Compiler | Command |
|---|---|---|
| [`testos9c-cc32.a`](test/os9c/testos9c-cc32.a) | **Microware C 3.2**, the original compiler (DOS-hosted cross version, run under vDos) | `xcc -a -s -bp testos9c.c` |
| [`testos9c-ucc25.a`](test/os9c/testos9c-ucc25.a) | **Microware Ultra C 2.5 in K&R compatibility mode** | `xcc -mode=compat -to=osk -tp=68k -a -c -s testos9c.c` |

Microware C 3.2's `-a` output is the compiler pass's (`c68`) output, from before the assembly
optimizer (`o68`) runs: compiling with and without `-o` (optimizer off) gives byte-identical
`.a` files. The calling convention and register-save masks come from `c68`, so the probe shows
the right stage. Optimized code could only be inspected by compiling to `.r` and disassembling,
which needs the ROF tooling (§6); a small research item for later.

Microware C 3.2 is the reference, since the CD-i libraries were built with it. Ultra C 2.5's
compat mode agrees with it on argument passing and most return values. Its column has five
entries: two real differences (float return and struct return, the rows with bold names) and three
harmless ones (restoring d0 in the callee, and registers it additionally saves or relies on).

Observed rules:

| Topic | Microware C 3.2 (reference) | Ultra C 2.5 compat, if different | Tests |
|---|---|---|---|
| Arg 1 | int/pointer → d0, double → d0:d1, struct → stack | | 01, 05, 08, 12 |
| Arg 2 | int/pointer → d1 **only if arg 1 went in d0**, otherwise stack | | 04, 07, 12, 13 |
| Other args | stack, pushed right to left, 4-byte slots; caller pops | | 02, 03 |
| `f(int, double, int)` | int in d0, double and last int on the stack, d1 unused | | 04 |
| `f(struct, int)` | both on the stack, d0 unused | | 12 |
| Structs by value | copied onto the stack (even 20 bytes), no hidden pointer | | 12–15 |
| Odd-sized structs | `sizeof` isn't padded (1, 2, 3, 5, 6, 7), but the stack slot is rounded up to a multiple of 4 bytes, and the struct sits at the *start* (lowest address) of its slot | | 50–62, 80–83 |
| Data layout | a `short`, `int` or `double` after a `char` is aligned to 2 bytes (16-bit alignment), `enum` is 4 bytes, and bitfields are allocated from the most significant bit. GCC's m68k layout is identical (checked with the same tests), so shared structs need no changes | | 70–79 |
| Floats (unprototyped) | promoted to double (`_T$FtoD`): in d0:d1 or 8 bytes on the stack; a callee receives a double | | 10, 11, 32 |
| Indirect calls | same convention as direct calls | | 16 |
| char/short arguments | caller widens, and the callee re-widens | | 09, 33 |
| char/short returns | **caller** widens the result (`ext.w`/`ext.l`); callee leaves it narrow | | 17–19, 34 |
| Pointer return | d0 (not a0) | | 20 |
| **float return** | float in d0: the callee returns it unconverted, and the caller converts it to double (`_T$FtoD`) only when needed, or stores d0 directly into a float | a **double** in d0:d1, K&R-style: the callee converts the result to double, and the caller treats d0:d1 as a double, converting back to float only when storing into a float | 21, 28, 29 |
| double return | d0:d1 | | 22 |
| a0, a1, d2, d3 | callee-saved (observed; a1 in the struct copies of tests 57, 61 and 83). The probe's Microware C 3.2 output never uses d4–d7; the manual says all registers except argument and return registers are preserved | also saves d4–d7 when used | 35, 36, 43–47, 57, 61, 83 |
| d1 (callee) | saved unless it carries an argument or the return value: `void f()` and one-int-arg/int-return functions both restore d1. Exception: a `float`-returning function doesn't restore d1 (`def28` clobbers it in `_T$FMul`), i.e. a float return is treated like a d0:d1 return, so callers can't rely on d1 surviving such a call | | 01, 28, 43, 44 |
| d0 (callee) | **never restored**: functions push d0 in the prologue, to give the first argument a stack slot (its "home slot"), but don't restore it, even in `void f()` | `void f()` saves *and restores* d0 | 43 |
| d1 (caller) | callers **rely** on d1 surviving a call when d1 carries no argument or return value: `keep47` keeps a value in d1 across an int-returning call (`keep40`–`keep42` reload their values from the stack instead) | also relies on d1 in `keep40`–`keep42`, and on pointers in a0/a1 surviving a call in `keep46` (a0 also in `keep47`) | 40–42, 46, 47 |
| **Struct return** | **static buffer**: the callee copies the result into a static (`vsect`) buffer and returns its *address* in d0; the caller copies from there. No hidden pointer is passed, and it isn't reentrant (the "PCC convention", from the old Portable C Compiler) | caller passes a buffer address in **a0** for *all* structs; arguments still use d0/d1; callee copies into `(a0)`, preserves a0, returns nothing in d0 | 24–27 |

So d0 is always caller-saved, while d1 is preserved unless it's used for an argument or the
return value. The two compilers' struct-return conventions are incompatible with each other, and
so are their float-return conventions, so no single scheme can work with both compilers for
functions returning a struct or a `float`.

The struct-return and float-return differences are **undocumented**. *Using Ultra C/C++* (chapter 2,
"Compatibility with the Microware K&R C Compiler" and "Differences Between Compatibility Source
Mode and the Microware K&R C Compiler") calls compat mode "logically equivalent" to the K&R
compiler, and lists the known differences:
- `remote` storage class
- keywords as struct member names
- stricter error checking
- name clashes
- the executive
- `csl` vs. `cio`
- K&R vs. `__ansi_*` library functions
- library size
- assembly escapes
- stdio macros with old ROFs

It even implies that Ultra C 2.5 code links with K&R-compiled ROFs, but it says nothing about
calling conventions. In practice, mixing works unless a function returning a struct or a `float`
crosses between the two compilers.

Two earlier GCC ports to OS-9 supported a similar convention, but neither matches it exactly:
- **GCC 1.37.1, by T. Shinohara and A. Seyama** (on the "CDISC 4" CD-i developer disc, 1993):
  the first 8 bytes of *named* arguments go in d0/d1 (`-mregparm`, the default), and unnamed
  arguments always go on the stack, like Ultra C 2.5. It treats d0, d1, a0 and a1 as scratch,
  and returns structs via a pointer in a1. It emits Microware assembler syntax directly.
- **GCC 2.6.2, by Walter Hunt** (`gcc-sun4-os9-2.6.2`): `-mregparm` keeps a0/a1 callee-saved,
  but assigns registers by byte offset, so for `int, double` it splits the double between d1 and
  the stack. Carl Kreider's 1997 patch, taken from Stephan Paschedag's OS-9 port of GCC 2.5.8,
  stops the splitting and keeps structs on the stack, but still assigns by byte offset, so a
  small struct as the first argument would leave the next int in d1.

Both contradict what the Microware compilers actually do in some cases; the design below follows
the observed behaviour.

## 4. `-mos9call` design

A new target option for the `m68k-elfos9` GCC, implemented in `gcc/config/m68k/`.

### Which functions use it

GCC function types do not record language linkage, so a function pointer cannot know whether
its target is `extern "C"`. The only consistent rule the backend can apply is:

> Under `-mos9call`, every call to, and definition of, a function of plain function type
> (`FUNCTION_TYPE`) uses the OS-9 convention, whatever its linkage. That includes free functions,
> static member functions, `extern "C"` functions, and all calls through plain function pointers.
> Non-static member functions (`METHOD_TYPE`, which take `this`) and compiler-generated library
> calls (libgcc helpers such as `__mulsi3` and soft-float routines) keep the normal stack
> convention.

The backend sees the function type on both sides (call sites, including indirect calls, and the
function being compiled), so this is a simple type check. It needs no name-mangling heuristics.

Further scope rules:

- **C++:** static member functions use the OS-9 convention, since they're ordinary functions.
  Non-static member functions (virtual functions, constructors, destructors, a lambda's
  `operator()`) keep the stack convention. Free functions with C++ linkage use the OS-9 convention
  too: that's unavoidable, because a plain function pointer can't tell them apart from C
  functions, and harmless. The function-pointer conversion of a capture-less lambda has plain
  function type, so such lambdas can serve as callbacks for Microware libraries. The toolchain
  currently builds C only, so this applies once C++ is enabled.
- **Interrupt handlers:** functions with the `interrupt_handler` (or `interrupt`) attribute are
  always excluded; they keep their own entry and exit conventions.
- **Per-function override:** two attributes allow mixing conventions within one program.
  `__attribute__((stackcall))` forces the stack convention under `-mos9call`, e.g. for runtime
  entry points defined in libgcc or in assembly (§4, "Libraries").
  `__attribute__((os9call))` selects the OS-9 convention without the flag, e.g. for declaring
  individual Microware library functions. Both are part of the function type, so they can be
  used in function-pointer declarations and typedefs, and pointers keep the right convention:
  `int (__attribute__((os9call)) *cmp)(const void *, const void *);`. Assigning a function or
  pointer of one convention to a pointer of another is diagnosed as an incompatible pointer type.
  The names are this project's own: GCC has no calling-convention attributes for m68k (only
  `interrupt`, `interrupt_handler` and `interrupt_thread`), and Microware documents none for 68K.
  They're modelled on GCC's x86 attributes. `stackcall` corresponds to x86's `cdecl` (arguments
  on the stack, the *caller* pops them), not to `stdcall`, where the callee pops the arguments
  (on m68k, that would be GCC's `-mrtd` convention).

  **C++ mangling:** GCC's C++ front end already mangles every type attribute that affects type
  identity, as written, as a vendor-extended qualifier (`U7os9call`, `U9stackcall`). To keep one
  mangling per type, the attribute handlers drop the redundant attribute: `os9call` under
  `-mos9call`, and `stackcall` without it. So only an attribute that differs from the flag's
  default appears in mangled names, and mangling depends on the flag: under `-mos9call`,
  stack-convention function types carry `U9stackcall`; without it, `os9call` types carry
  `U7os9call`. Code compiled without `-mos9call` that doesn't use `os9call` mangles exactly as
  with the regular m68k ELF compiler. No changes to the C++ front end are needed.

  Two limits: a function's own convention isn't part of its mangled name (only the types of
  pointer parameters are), so mismatched declarations in different files link silently; and the
  meaning of an unattributed type depends on the flag.

### Arguments

Positional, as observed (§3). Arguments are classified by their *type*, not by GCC's machine
mode: small structs and unions have integer modes in GCC, but they're aggregates and always go
on the stack.

- arg 1: ≤ 4-byte scalar (int, long, pointer, widened char/short, float) → d0; 8-byte scalar
  (double, `long long`) → d0:d1; anything else → stack. That includes aggregates of any size,
  `long double` (12 bytes) and `_Complex` types
- arg 2: ≤ 4-byte scalar → d1, only if arg 1 used d0 alone (not d0:d1, not the stack); otherwise
  stack
- everything else on the stack, pushed right to left, caller pops; no stack space is reserved
  for register arguments
- an aggregate on the stack takes a slot rounded up to a multiple of 4 bytes, and sits at the
  *start* of it (padding after the data). GCC currently right-justifies aggregates smaller than 4
  bytes (a 1-byte struct at offset 3 of its slot, a 2-byte one at offset 2, a 3-byte one at
  offset 1), so `-mos9call` must pad them upward instead.
  From 5 bytes up, GCC already matches. GCC's padding hook doesn't know the call's convention,
  so the upward padding applies to all calls under `-mos9call`, and passing an aggregate smaller
  than 4 bytes by value to a function of the other convention (a `stackcall` function under the
  flag, an `os9call` one without it) is an error. A 1-byte struct is pushed by m68k's `pushqi1`
  pattern, which normally stores the byte at offset 1; under the flag it stores it at offset 0.

**Widened prototypes.** This applies only at the boundary with Microware-compiled code. Between
GCC-compiled functions, prototyped `char`, `short` and `float` parameters, and `float` return
values, work normally under `-mos9call`: both sides pass a `float` as 32 bits, in d0, d1 or a
4-byte stack slot, and return it in d0.

Microware C 3.2 functions are all defined K&R-style, so they receive their arguments in promoted
form: `char` and `short` as `int`, `float` as `double`. By the C standard, a prototype is only
compatible with such a definition if it uses those promoted types. Prototypes *for Microware C 3.2
functions* should therefore use `int` (or `unsigned`) and `double`, never `char`, `short` or
`float`: `extern "C" int f(double x, int c);` rather than `(float x, char c)`. GCC can't tell which
declarations refer to Microware functions, so it doesn't warn about this; getting such
prototypes right is up to whoever writes them.

- **`float` parameters would break** when calling a Microware C 3.2 function: GCC passes a
  prototyped `float` as 32 bits, while the callee expects a double.
- **`char`/`short` parameters happen to work:** m68k GCC widens them to a full 32-bit
  slot or register at call sites (`TARGET_PROMOTE_PROTOTYPES`), as Microware callers do, but
  `int` is the correct, portable form.
- **Return types:** functions *written* to be called from, or to call into, Microware-compiled code
  should return `double` rather than `float`. Microware C 3.2, Ultra C 2.5 and GCC `-mos9call` all
  return a double the same way (in d0:d1), while they disagree on `float` (see "Return values").
  Declarations of *existing* functions must still match their definitions: a prototype can't change
  how a function returns.

**`long long`** follows the double rule (d0:d1 as argument 1, otherwise the stack; returned in
d0:d1). This is GCC-only, since Microware C 3.2 has no `long long`.

### Variadic functions

**Background.** Microware C 3.2 has no prototypes and no `stdarg.h` or `varargs.h`. Its callers
can't know that a function is variadic, so `printf(fmt, x)` is compiled like any other call (`fmt`
in d0, `x` in d1). Microware C 3.2's variadic library functions must therefore expect that. How they
read their arguments is undocumented. The usual K&R trick of walking from the address of the last
named parameter can't work: Microware C 3.2's prologue stores d0/d1 below the frame pointer,
separated from the stack arguments by the saved a5 and the return address. So they're presumably
written in assembly, or use a library-specific mechanism. Code from that era supplied its own
varargs support. For example, a CD-i developer's header set from the mid-1990s has only a
placeholder `stdarg.h`, and its formatting routines rely on a separate, private `varargs.h`.

Ultra C 2.5, which has prototypes, documents a different rule (*Ultra C/C++ Processor Guide*, 68K,
"Passing Arguments to Functions"): named arguments follow the d0/d1 rules, and all variable
arguments go on the stack. Its `stdarg.h` relies on that: `va_list` is a plain pointer, `va_start`
uses a compiler intrinsic to point at the first unnamed argument, and `va_arg` steps through the
stack in 4-byte units.

**Rule for `-mos9call`: variadic calls are passed like unprototyped K&R calls.** The positional
rules apply to *all* arguments, named or not, and the default argument promotions (`float` →
`double`, `char`/`short` → `int`) apply to the unnamed ones as usual. So:

- `g(int a, ...)` called as `g(1, x)`: `a` in d0, then `x` in d1 if it's an int or pointer (a
  double `x` goes on the stack)
- `h(int a, int b, ...)` called as `h(1, 2, x)`: d0, d1, then `x` on the stack
- `f(...)` with no named parameter: the first arguments follow the same rules as any call. Only
  C++ accepts this declaration in GCC 11; C allows it from C23, which GCC supports from GCC 13.
  Before C++26 and C23, the callee can't use `va_start` without a named parameter, so in practice
  it's a declaration for functions implemented elsewhere, which is exactly the Microware case.
  In C, the unprototyped `f()` gives the same calls.

This matches every Microware C 3.2 caller, and Microware C 3.2's variadic library functions too
(confirmed by running them, see below). It deliberately differs from
Ultra C 2.5: calling a variadic function compiled with it from `-mos9call` code would put the
first unnamed arguments in the wrong place.

Dual ANSI/K&R headers from that era show why this matters in practice. They wrap declarations in
a prototype macro that expands to the full parameter list for an ANSI compiler and to `()` for
Microware C 3.2. So GCC sees real variadic prototypes (`printf`-style functions, file creation
with optional arguments, and so on), while the implementations behind them were compiled K&R-style
and expect positional arguments. Passing variadic calls like K&R calls is exactly what makes GCC
calls to those functions work.

**Callee side.** A variadic function *defined* with `-mos9call` must find its first arguments in
d0/d1. The current m68k `va_list` (a pointer walking up the stack) can't do that, because the
register arguments aren't contiguous with the stack ones. It needs a target-specific `va_list`,
built with GCC's standard hooks (`TARGET_SETUP_INCOMING_VARARGS` to save d0/d1,
`TARGET_BUILD_BUILTIN_VA_LIST`, `TARGET_EXPAND_BUILTIN_VA_START`, `TARGET_GIMPLIFY_VA_ARG_EXPR`),
as x86-64 and PowerPC have:

- the prologue of a variadic function saves d0/d1 to a register save area
- `va_list` records the next argument's position, what the first argument was (int/pointer in
  d0, double in d0:d1, or on the stack), and the next stack argument
- `va_arg` applies the positional rules: position 0 takes an int or pointer from d0 or a double
  from d0:d1, position 1 takes an int or pointer from d1 only if position 0 used d0 alone, and
  everything else comes from the stack

**Staging.** The caller side is simple and more important, since it lets GCC code call
Microware C 3.2's `printf` and friends. It can come first. Until the callee side exists, defining a
variadic function with the OS-9 convention (under `-mos9call`, or with an explicit `os9call`
attribute) is rejected with an error, rather than silently miscompiled.
This also means newlib's stdio can't be built with the flag until the callee side exists (see
"Libraries").

**Supporting evidence from an earlier port.** The GCC 1.37.1 OS-9 port by T. Shinohara and
A. Seyama (§3) ships a `varargs.h` "for GCC and MicroWare C". Its `va_dcl` declares the first
parameter as a `double`, followed by an `int`: by the Microware rules, a `double` first argument
arrives in d0:d1, so the callee has d0 and d1 next to each other in memory, and `va_arg` walks
through those 8 bytes before moving on to the stack arguments. That is consistent with the
positional rule above for integral arguments: a K&R caller passes `printf(fmt, x)` as `fmt` in d0
and `x` in d1. It doesn't cover every case (a double as the second argument is on the stack, not
in d1), and that port's own callers put unnamed arguments on the stack, so it's supporting
evidence rather than proof. It's also a simple model for GCC's callee side: save d0/d1, then
continue with the stack arguments.

**Confirmed by running the library:** with the CD-i `clib.l` converted by `rof2elf` (§6) and linked
with `-mos9call` code, Microware C 3.2's `sprintf` and `sscanf` read their arguments correctly
when called with this positional placement: `sprintf(buf, "%d %s %x %c", -7, "str", 255, 'z')`
gives `-7 str ff z` (Level 3, §11), and a double as an unnamed argument is read from the stack,
high half first: `sprintf(buf, "%d %x %x %d", 7, 1.5, 9)` gives `7 3ff80000 0 9`. Not yet
checked: a double as the second argument of a variadic call (on the stack, not in d1), which needs
a library function with one named parameter, such as `printf`, and so OS-9 for its I/O.

### Return values

- **Scalars need no change.** On `elfos9`, `m68kemb.h` (included after `m68kelf.h`) makes
  `FUNCTION_VALUE` use `LIBCALL_VALUE`, so pointers already return in d0 and 64-bit values in d0:d1.
  (Returning pointers in a0 only applies to the Linux and NetBSD targets.) Narrow values may stay
  unwidened, because Microware callers widen them themselves.
- **float** is returned as a float in d0, as Microware C 3.2 does. Ultra C 2.5 returns and expects
  a double in d0:d1 instead (§3, tests 28–29), but Microware C 3.2 is the reference. It's also
  the faster choice: the Ultra C 2.5 way costs a float-to-double conversion in the callee, and
  often one back in the caller, which on the CD-i's FPU-less 68070 are software routine calls
  (and trapped, emulated instructions with `-m68881`). As with structs, the Microware C 3.2
  headers declare no `float`-returning functions (the math functions return `double`), so this
  difference doesn't affect library interoperability.
- **With `-m68881`**, GCC normally returns float and double in fp0. Under `-mos9call` they stay in
  d0 (float) and d0:d1 (double), as Ultra C 2.5 does even when it uses the FPU.
- **`long double`, `_Complex` and vector types** are returned in memory, like structs (buffer
  address in a0). Vectors are passed on the stack too.
- **Structs.** The two Microware compilers are incompatible here (§3), so `-mos9call` had to pick
  one:
  - **Microware C 3.2 (static buffer, PCC convention), not chosen:** the callee returns the address
    of a static copy in d0. It matches the CD-i libraries' compiler, but isn't reentrant. GCC
    supports it only through the compile-time macro `PCC_STATIC_STRUCT_RETURN` (`calls.c`,
    `function.c`; used by `m68k/openbsd.h`), which can't be switched per option without core
    changes.
  - **Ultra C 2.5 (buffer address in a0), chosen:** reentrant, and close to what GCC already does.
    `m68kelf.h` sets the struct-value register to a0, but `m68kemb.h` sets
    `DEFAULT_PCC_STRUCT_RETURN 0`, so GCC returns small structs that have an integer mode (1, 2, 4
    or 8 bytes, suitably aligned) in d0/d1. Under `-mos9call`, the return-in-memory hook returns
    true for all aggregates.

  For library interoperability the choice doesn't matter: the Microware C 3.2 headers declare no
  function that returns a struct or union by value, either directly or through a typedef, and
  `div()`/`ldiv()` aren't provided at all. Other headers from that era checked so far show the
  same. The choice only affects user code compiled by the two compilers calling each other.

  **Decision:** the Ultra C 2.5 a0 convention. It's reentrant, close to what GCC already does,
  and needs no changes to GCC's core. The Microware C 3.2 static buffer would only matter for
  third-party Microware C 3.2 objects that return structs.

### Register preservation

Microware C 3.2 callers rely on d1 surviving calls, and callees preserve a0, a1 and d2–d7 (per the
manual and the observed output), while d0 is never preserved. So:

- An **os9call function (callee)** saves a0/a1 when it uses them or makes calls. That includes a0
  when it carried the struct-return buffer address, as in Ultra C 2.5. It also saves d1 when it
  uses d1 or makes calls, unless d1 carries an incoming argument or the return value; the incoming
  argument state tells which. d0 needn't be saved (Microware C 3.2 never restores it; Ultra C 2.5
  does, which is harmless). This reuses the `interrupt_handler` logic in `m68k_save_reg`.
- **FPU registers** (with `-m68881`): an os9call callee saves every FPU register it uses,
  including fp0/fp1, which GCC normally treats as scratch. Ultra C 2.5 does the same.
- **GCC callers** keep assuming d0/d1/a0/a1 are clobbered, which is always safe. A later
  optimisation could let callers rely on a0/a1 surviving calls to os9call functions, via GCC's
  per-function register-save support (`TARGET_FNTYPE_ABI` / `TARGET_INSN_CALLEE_ABI`).
- **Tail calls (sibling calls)** from os9call functions are disabled. Otherwise a non-os9call
  callee could clobber a0/a1 (or d1) after the epilogue has restored them.

### Options and special cases

| Case | Behaviour under `-mos9call` | Why |
|---|---|---|
| `-mshort` (16-bit `int`) | rejected with an error | Microware C 3.2's `int` is 32 bits; argument slots and widening rules assume it |
| `-m68881` | supported, with a warning | OS-9 traps FPU instructions and emulates them, so the code runs on a CD-i's 68070, but slowly. Return values stay in d0/d0:d1 and callees save the FPU registers they use (above) |
| no `-ma6rel` | warning | Microware code expects a6 to point at the static storage area, and `-ma6rel` keeps a6 reserved for that |
| nested functions (GCC extension) | allowed; error when a trampoline would be needed | Taking a nested function's address makes GCC build a trampoline that loads the static chain into a1 before the function can save it. A Microware caller of such a pointer (e.g. a `qsort` comparator) would lose a1. Direct calls are fine |
| `-mrtd` (callee pops the arguments) | rejected with an error | the OS-9 convention has the caller pop the arguments |
| frames of 32 KB or more | a1 is always saved in such functions, with a test | GCC's m68k epilogue then uses a1 as an index register while restoring registers. With `movem` that's safe once a1 is in the save mask (the address is computed before loading); the hazard is only in the register-by-register restore path, used when fewer registers are saved |
| `-fstack-limit-symbol` | rejected with an error (with `-m68020` and up) | GCC already disables `-fstack-limit-*` on the 68000/68070 (its check uses `TRAPcc`, a 68020 instruction). On the 68020, the check loads the limit into d0 when it isn't a simple constant (in practice always, under `-mpcrel`), clobbering argument 1. See also "Stack checking" below |
| `-fstack-limit-register` with d0 or d1 | rejected with an error (with `-m68020` and up) | the limit register would collide with the argument registers |
| `-fuse-cxa-atexit` (C++) | rejected with an error | the destructor passed to `__cxa_atexit` is a member function (`METHOD_TYPE`, stack convention), but it's called through a `void (*)(void *)` pointer, which is os9call, so `this` would arrive in d0. `thread_local` destructors always go this way, so they're unsupported |
| `-mos9stkchk` | requires `-ma6rel` (error without it); skipped in `interrupt_handler` functions | `_stkcheck` and its limit live in Microware's a6 static area; interrupt handlers don't run on the process stack. See "Stack checking" |
| `-malign-int`, `-fshort-enums`, `-fpack-struct` | warning | they change the data layout, which otherwise matches Microware C 3.2 exactly (§3) |
| `-pg`, `-finstrument-functions` | rejected for now | the profiler call sequence clobbers a0, and `mcount` and the instrumentation hooks would have to preserve the incoming d0/d1 |
| `-fprofile-arcs`, `-fprofile-generate` | rejected for now | they call libgcov (`__gcov_init`, the `__gcov_*_profiler` routines) through declarations with plain function types, so under the flag those calls would be os9call, but libgcov is built without it |
| `__builtin_apply`, `__builtin_apply_args`, `__builtin_return` | unsupported for now (error, once per file) | they forward arguments by saving all possible argument and return registers; untested with d0/d1 arguments, and rarely used. As implemented, the error comes from the raw-mode hooks (`TARGET_GET_RAW_ARG_MODE`/`TARGET_GET_RAW_RESULT_MODE`), which only these built-ins use |
| other m68k targets | `-mos9call` is an error, the attributes are ignored with a warning | return values and struct return are only handled for `elfos9` (`M68K_OS9_TARGET` in `m68k-os9.h`) |

**Callbacks and entry points.** Code called from outside GCC-compiled code must use the matching
convention, which `-mos9call` provides:
- `main` is called by the startup code. Microware's standard `cstart.r` passes `argc` and `argv`
  in d0/d1, which matches an os9call `main`. A custom startup routine must do the same, or call
  `main` with the convention `main` was compiled for.
- Callbacks passed to Microware or newlib code (`qsort` comparators, `atexit` functions) and
  handlers called by the runtime or OS-9 (signal and intercept handlers) are ordinary functions,
  so they use the OS-9 convention, as their callers must expect.

The test plan covers each of these (§11).

### Stack checking

Microware C 3.2 checks the stack in every function by default (`cc -s` turns it off; the probes in
[`test/os9c/stkchk.c`](test/os9c/stkchk.c) show both compilers' code, compiled like `testos9c.c`
in §3 but without `-s`: `xcc -a -bp stkchk.c` for Microware C 3.2 and
`xcc -mode=compat -to=osk -tp=68k -a -c -bp stkchk.c` for Ultra C 2.5). The two Microware
compilers use different, incompatible mechanisms:

| | Microware C 3.2 (`stkchk-cc32.a`) | Ultra C 2.5 (`stkchk-ucc25.a`) |
|---|---|---|
| check | `move.l #-N,d0` then `bsr _stkcheck` | `sub.l #N,_stklimit(a6)`, and `bsr _stkhandler` on a borrow; `add.l #N,_stklimit(a6)` again on exit |
| placement | after `link` and the register saves, before the locals are allocated | after the prologue |
| `N` | local-variable size plus a margin: 64 bytes in a leaf function, 68 in one that makes calls; saved registers don't count, and calls to runtime helpers (`_T$DAdd`) don't make a function non-leaf | roughly the function's stack use |
| registers | overwrites d0 (the `-N` argument); the function then reloads its arguments from their home slots (§3). It never relies on d1 surviving the call either (probe functions `leaf2`, `bigframe2`, `leafd`) | keeps d1 in its register across the check (`_stkhandler` is only called on overflow) |

GCC's own stack-limit check (`-fstack-limit-*`) uses `TRAPcc`, a 68020 instruction, so GCC
disables it on the 68000/68070. For CD-i programs, whose startup code (`cstart.r`) and libraries
come from Microware C 3.2, the useful mechanism is Microware C 3.2's: GCC can emit the same
`move.l #-N,d0` / `bsr _stkcheck` sequence on the 68000. It's enabled by a new opt-in option,
**`-mos9stkchk`** (off by default, unlike Microware C 3.2), so GCC and Microware code share one
stack limit and one overflow handler. The option needs Microware's `cstart.r` and `clib.l`, which
provide `_stkcheck` and the limit it checks, so it depends on `rof2elf` (§6); the README's own
`cstart.s` provides neither. It requires `-ma6rel` and skips `interrupt_handler` functions (see
"Options and special cases").

GCC doesn't always allocate the locals in `link` (large frames use `link a5,#0` followed by
`add.l`, and without a frame pointer it uses `lea` or `sub`), so it emits the check at the start
of the prologue, before the frame is allocated. Microware C 3.2's margin is what's left *after*
`link` and the register saves, so GCC counts those too:
`N` = frame size + register-save area + 4 (for `link`, if any) + 64, or + 68 if the function
makes calls. GCC treats a function that only calls libgcc helpers (`__mulsi3`, soft-float) as
making calls, where Microware C 3.2 uses 64; that's harmless. When `N` ≤ 128, `moveq #-N,d0` is
enough.

Microware C 3.2 saves registers first and checks afterwards, so whatever `_stkcheck` clobbers has
already been saved. GCC checks first, so until `_stkcheck`'s contract is known, it saves around
the check every register `_stkcheck` might clobber that is live on entry or must be preserved for
the caller: for os9call functions d1 always (it's callee-saved, §4 "Register preservation"), and
d0 when it carries an argument; for any function, a0 when it carries the struct-return address
and a1 when it carries the static chain (e.g. `movem.l d0/d1,-(sp)` before the check and
`movem.l (sp)+,d0/d1` after). If `_stkcheck` turns out to preserve everything except d0, only d0
needs saving. The rest of its contract (other registers, what it does on overflow) is a research
item: disassemble it with the ROF tooling (§6).

`_stkcheck` also keeps a low-water mark of the stack pointer, which `freemem()` and `stacksiz()`
report; those are only accurate if all code is built with stack checking.

### Implementation points (`gcc/config/m68k/`)

Insertion points in the GCC 11 tree (`gcc/config/m68k/`; line numbers approximate):

| Where | GCC's purpose | What it must achieve |
|---|---|---|
| `m68k.opt` (next to `ma6rel`) | option definitions | new target masks for `-mos9call` and (later) `-mos9stkchk`, both negatable (`-mno-os9call`); also document them in `gcc/doc/invoke.texi`, m68k options |
| `m68k.h` ~60, `TARGET_CPU_CPP_BUILTINS` | predefined macros | define `__OS9CALL__` under `-mos9call` |
| `m68k.h` ~495, `CUMULATIVE_ARGS` | per-call argument-scanning state | remember whether the call uses the OS-9 convention, the argument position, and where arg 1 went (d0, d0:d1 or the stack) |
| `m68k.h` ~498, `INIT_CUMULATIVE_ARGS` | runs per call site and for the function being compiled | decide the convention from the function type and its `stackcall`/`os9call` attribute; libcalls (no type), `METHOD_TYPE`, `interrupt_handler` functions keep the stack convention; built-in declarations get it from their `stackcall` type (see "Libraries"). One predicate, given the function type, makes this decision for every hook. (`m68k.h` names the 4th argument of `INIT_CUMULATIVE_ARGS` `INDIRECT`, but `calls.c` passes the function declaration.) |
| `m68k.h` ~491, `FUNCTION_ARG_REGNO_P` | which registers can carry arguments | admit d0 and d1. This is global: it feeds dataflow entry definitions, alias analysis, if-conversion and loop invariants. As implemented, it admits them under `-mos9call` or once an `os9call` attribute has been applied in the translation unit, so code using neither is unchanged (Level 0 confirms it) |
| `m68k.c` ~1447, `m68k_function_arg` | where an argument goes (register, or stack) | the positional rules of §3/§4, applied to named and unnamed arguments alike (variadic calls behave like unprototyped calls) |
| `m68k.c` ~1453, `m68k_function_arg_advance` | updates the state after each argument | track position and register use; stack byte counting must keep working |
| `m68k.c`, `TARGET_FUNCTION_ARG_PADDING` (new) | how arguments smaller than their slot are padded | under `-mos9call`, pad aggregates upward (data at the start of the slot); `pushqi1` in `m68k.md` follows it for 1-byte structs |
| `m68k.c` ~294/~5928, `TARGET_RETURN_IN_MEMORY` | which return types go through memory | defined under `M68K_HONOR_TARGET_STRICT_ALIGNMENT`, which is 1 by default (only `linux.h` sets 0), so the hook is active on `elfos9`; under `-mos9call`, all aggregates, `long double` and `_Complex` types (the Ultra C 2.5 a0 convention, §4) |
| `m68k.c` ~917, `m68k_save_reg` | which registers the prologue saves | a0/a1 when used or non-leaf; d1 unless it carries an incoming argument or the return value (d0 never); model it on the `interrupt_handler` branch, using `crtl->args.info` for incoming arguments |
| `m68k.c` ~1401, `m68k_ok_for_sibcall_p` | whether a tail call is allowed | refuse for os9call functions |
| `m68k.c`, `m68k_epilogue_uses` (`EPILOGUE_USES`) | which registers count as live at function exit | under the OS-9 convention, every call-clobbered register except d0 (as for interrupt handlers); otherwise the epilogue's restores of d1, a0 and a1 are deleted as dead (found by the Level 1 tests) |
| `m68k.c`, `m68k_attribute_table` | target attributes | add `stackcall` and `os9call` as *type* attributes (like x86's `stdcall`, unlike m68k's declaration-only `interrupt*` attributes), with `affects_type_identity` set, so they can appear in function-pointer types and typedefs |
| `m68k.c`, attribute handlers (C++ mangling) | how function types appear in C++ mangled names | nothing to add for mangling itself (the C++ front end mangles identity-affecting type attributes as written); the handlers drop the attribute that matches the flag's default (`os9call` under `-mos9call`, `stackcall` without it), so each type has one mangling |
| `m68k.c`, `TARGET_COMP_TYPE_ATTRIBUTES` (new) | whether two function types' attributes are compatible | function types with different *effective* conventions are incompatible (no attribute means `os9call` under `-mos9call` and `stackcall` without it), so assigning between them is diagnosed, and in C++ they're distinct types |
| `m68k.c`, `TARGET_FUNCTION_VALUE` (new) | where return values go | for os9call functions, all values in d0 / d0:d1, float and double too, even with `-m68881`; otherwise the existing `FUNCTION_VALUE` macro (on `elfos9`, `m68kemb.h` maps it to `LIBCALL_VALUE`). It has to be the hook, not the macro: the macro only gets the declaration, so indirect calls would miss the convention (found by the implementation review). `m68k_libcall_value` stays unchanged |
| `m68k.c`, `m68k_option_override` | option sanity checks | the option rules in "Options and special cases": reject `-mshort`, `-mrtd`, `-pg`, `-finstrument-functions`, `-fprofile-arcs`, `-fprofile-generate`, `-fuse-cxa-atexit`, `-fstack-limit-symbol` and `-fstack-limit-register` with d0/d1, and `-mos9stkchk` without `-ma6rel`; warn for `-m68881`, missing `-ma6rel`, and the layout-changing `-malign-int`, `-fshort-enums` and `-fpack-struct` |
| `m68k.c`, `TARGET_INSERT_ATTRIBUTES` (new) | adds attributes to every declaration as it's created, GCC's built-in declarations included | under `-mos9call`, add `stackcall` to declarations and typedefs of the names listed in "Libraries" that have no convention attribute: built-ins (by library name), headers, definitions and K&R redeclarations alike |
| `m68k.c`, `TARGET_SETUP_INCOMING_VARARGS` (new) | called for each variadic function definition | until the callee side exists, an error for variadic functions with the OS-9 convention |
| `m68k.c` ~7065, `m68k_trampoline_init` | builds a nested function's trampoline | an error for nested functions with the OS-9 convention |
| `m68k.c`, `TARGET_GET_RAW_ARG_MODE`/`TARGET_GET_RAW_RESULT_MODE` (new) | register modes for `__builtin_apply_args`, `__builtin_apply`, `__builtin_return` | an error under `-mos9call`, once per file |
| `m68k.c` ~1036, `m68k_expand_prologue` (later, `-mos9stkchk`) | emits the prologue | the `_stkcheck` sequence of "Stack checking", at the start, with its register saves; not in `interrupt_handler` functions |

Background in the GCC internals manual: "Passing Arguments in Registers", "How Scalar Function
Values Are Returned", "How Large Values Are Returned", "Function Entry and Exit".

Implementation phases, in order. Each is checked against `test/os9c/testos9c-cc32.a`, except struct
return, which follows Ultra C 2.5 and is checked against `test/os9c/testos9c-ucc25.a`:
1. **option, attributes, state and init:** the `mos9call` option and its option checks,
   `__OS9CALL__`, the `stackcall`/`os9call` type attributes with type compatibility and C++
   mangling, `CUMULATIVE_ARGS` and the convention decision, the `stackcall` retyping of built-in
   declarations and of user redeclarations, and the errors for trampolines and
   `__builtin_apply`. No change without the flag.
2. **argument placement**, including upward padding of small aggregates, and the error for
   variadic definitions
3. **callee register saves**, including FPU registers with `-m68881`
4. **return values:** struct return (all aggregates in memory, buffer address in a0), float and
   double in d0 / d0:d1 also with `-m68881`, `long double` and `_Complex` in memory
5. **tail calls** disabled from os9call functions (as implemented, done together with phase 3: once
   the epilogue restores d1, a0 and a1, a tail call would restore d1 after loading the outgoing
   arguments; tail calls from stack-convention functions to os9call ones are refused too, since
   argument and return registers differ)

A predefined macro, `__OS9CALL__`, tells headers whether `-mos9call` is in effect (see "Headers
for Microware libraries").

### Libraries

If application code is built with `-mos9call`, every library it calls through ordinary function
calls must use the same convention: newlib (`printf`, …) and, for C++, libstdc++/libsupc++. That
means building them with the flag, as a multilib or by making the convention the default for the
target. newlib is configured separately in the `Dockerfile`, with its own `CFLAGS_FOR_TARGET`,
so adding the flag there is easy. Deferred. These cases need care:

- **libgcc must be built *without* `-mos9call`.** The compiler calls its helpers (`__mulsi3`,
  soft-float routines, …) as libcalls, which keep the stack convention, but their *definitions*
  are ordinary functions and would switch convention if compiled with the flag. The `Dockerfile`
  passes `CFLAGS_FOR_TARGET` to the GCC build, and so to libgcc, so the flag can't simply go there.
  A `mos9call` multilib, or making the convention the default, would also build libgcc and
  libgcov with it, so libgcc's target makefile fragment adds `-mno-os9call` (e.g. through
  `HOST_LIBGCC2_CFLAGS`). `MULTILIB_REUSE` can then avoid building libgcc twice.
- **Which functions keep the stack convention.** The rule: a function is `stackcall` if its
  definition, or one of its callers, is outside `-mos9call` code (libgcc, assembly). Checked with
  `nm -u` on the `elfos9` `libgcc.a`, that gives:
  - `memcpy` and `memset`: newlib defines them in m68k assembly (`memcpy.S`, `memset.S`), and
    libgcc calls them
  - `strlen`, `malloc` and `free`: newlib defines them in C, but libgcc calls them (the DWARF
    unwinder and emulated TLS)
  - entry points defined in libgcc or libatomic (`_Unwind_Resume`, `__atomic_*`, …), including the
    complex multiply and divide helpers (`__mulsc3` … `__divxc3`), which GCC also calls through
    built-in declarations (found by the implementation review)
  - `abort` (also called by libgcc) takes no arguments and never returns, so either convention
    works; it's listed for type consistency

  `memmove` and `memcmp` aren't called by libgcc and newlib defines them in C, so they stay
  unattributed (os9call under the flag). There is no `atexit` registration from startup code: the
  `elfos9` install has no `crtbegin`/`crtend`.
- **Microware's `clib` instead of newlib: `-mbuiltin=os9call`.** The list above is right for newlib.
  Microware's `clib.l` defines `memcpy`, `memset`, `strlen`, `malloc`, `free` and `abort` with the
  OS-9 convention, so a program linked against it needs them as os9call, including GCC's own
  `memcpy` calls for block moves (found when linking `rof2elf`-converted `clib`; see §6).
  `-mbuiltin=os9call` selects that; `-mbuiltin=stackcall` is the default. libgcc's own entry
  points keep the stack convention either way; its DWARF unwinder and emulated TLS, which call
  these functions, can't be used with `-mbuiltin=os9call`.
- **Runtime entry points called through built-in declarations.** GCC calls some of these
  functions through its own built-in declarations, which have a real function type, so
  `-mos9call` callers would use the OS-9 convention for them. Marking their *definitions*
  `stackcall` wouldn't help, because callers only see the built-in declaration.
- **One mechanism for all declarations (as implemented).** GCC passes every declaration through
  `TARGET_INSERT_ATTRIBUTES` when it's created, including its own built-in declarations (both
  `__builtin_memcpy` and the plain `memcpy`, and `_Unwind_Resume`, which is created late, in
  `build_common_builtin_nodes`). Under `-mos9call` the backend adds `stackcall` there to every
  declaration of the functions above that has no convention attribute: built-ins (recognized by
  their library name), header declarations, definitions, and old code's own redeclarations such
  as K&R `char *malloc();` (which GCC would otherwise only warn about, and then call with the
  wrong convention). So headers, built-ins, definitions and function pointers (`&memcpy`) all
  agree, and GCC keeps treating the functions as built-ins. newlib's headers and C definitions
  need no changes, and its m68k assembly versions stay as they are.
- **Other assembly sources don't follow the flag.** newlib's m68k `setjmp.S` reads its arguments
  from the stack, so `setjmp`/`longjmp` will need `stackcall` declarations (or os9call variants)
  when newlib is built with the flag (§12).
- **libsupc++** (built with the flag) calls `_Unwind_*` through the declarations in `unwind.h`,
  which aren't built-ins. Calls also go the other way: the unwinder calls the personality routine
  (`__gxx_personality_v0`) and the exception cleanup function through function pointers. The
  same mechanism covers these: typedefs whose names start with `_Unwind_` (such as
  `_Unwind_Personality_Fn`) get `stackcall` on their function type, and so do declarations of
  `_Unwind_*` functions and of the personality routines.
- **Variadic functions block newlib's stdio for now.** Defining variadic functions under
  `-mos9call` is an error until the callee side exists (see "Variadic functions"), so newlib's
  `printf` family can't be built with the flag before that. Since newlib also uses these
  functions internally, `-mos9call` programs effectively can't use newlib at all until then. The
  supported early mode is therefore: no `-mos9call`, plus `os9call` declarations (e.g. via
  `_OS9PROTO`) for the Microware library functions a program calls.

### Headers for Microware libraries

The `os9call` attribute also works without `-mos9call`, so a header can mark the Microware
library functions it declares, and programs that otherwise keep GCC's stack convention can still
call them. A prototype macro, `_OS9PROTO`, keeps such a header usable by Microware C 3.2 too:

```c
#ifdef __GNUC__
#if defined(__has_attribute)
#if __has_attribute(os9call)
#define _OS9PROTO(p) p __attribute__((os9call))    /* GCC with OS-9 call support */
#endif
#endif
#ifndef _OS9PROTO
#error "this GCC doesn't support the os9call attribute"
#endif
#else
#ifdef __STDC__
#define _OS9PROTO(p) p                             /* other ANSI compilers: see below */
#else
#define _OS9PROTO(p) ()                            /* Microware C 3.2 (K&R) */
#endif
#endif

int create _OS9PROTO((char *name, int mode, int perm, ...));
int _errmsg _OS9PROTO((int nerr, char *msg, ...));
```

- **GCC with OS-9 call support:** each declared function gets the `os9call` attribute, whether or
  not `-mos9call` is in effect. With the flag the attribute is redundant, but harmless.
- **Microware C 3.2** gets K&R declarations (`()`). The macro uses only `#ifdef`, `#ifndef`,
  `#else` and `#endif` outside the GCC branch, since that compiler predates `__has_attribute` and
  may lack `#elif`; whether its preprocessor skips the GCC branch cleanly is to be confirmed.
- **Other ANSI compilers** get plain prototypes. For Ultra C 2.5 in ANSI mode that is wrong for
  variadic functions, since it puts unnamed arguments on the stack.
- **A GCC without `os9call` support** stops with an error rather than silently using the stack
  convention.
- **Detection:** `__has_attribute(os9call)` (available since GCC 5) checks whether the attribute
  is supported, independently of the flag. `__OS9CALL__` tells whether `-mos9call` itself is on.
- **Variadic functions** such as `create` and `_errmsg` are called with the same positional,
  K&R-style placement as under the flag (see "Variadic functions").
- **Name clashes:** don't declare functions this way that newlib also declares (`printf` and the
  rest of the standard library): that conflicts with newlib's own declarations, and both
  libraries would define them. The pattern is for OS-9- and CD-i-specific functions.
- **Widened prototypes** still apply: the parameter lists inside `_OS9PROTO` must use `int` and
  `double`, never `char`, `short` or `float` (see "Arguments").

`_OS9PROTO` is this project's own pattern for headers that declare Microware library functions.
Dual ANSI/K&R headers from the era used similar prototype macros, without the attribute.

## 5. How dependent is the toolchain on ELF?

Very little of it is in the compiler. GCC never writes object files: `cc1` emits assembly text,
and `as` turns it into an ELF `.o`.

| Piece | Tied to ELF? |
|---|---|
| `cc1` (GCC) | Only through the assembly syntax and section names it prints, which come from the target headers (`m68kelf.h`, `elfos.h`, formerly `dbxelf.h`) |
| GNU `as` / `ld` (BFD) | Yes. Modern BFD cannot read or write ROF |
| Linker script (`.data` at -0x8000, `-Wl,-q`) | Yes, an ELF link convention |
| `elf2mod` | Yes, it turns a linked ELF into an OS-9 module |

The ELF-specific things in the target headers are:
- directives: `.section`, `.type`/`.size`, `.weak`, `.hidden`, `.comm`/`.local`, `.ident`
- `.L` local labels and the `%` register prefix
- section names: `.rodata`, `.text.startup`, `-ffunction-sections`, `.init_array` for C++
  constructors
- DWARF debug info

The code generator itself barely cares, and the fork's own changes concern addressing, not the
object format.

Options for working with Microware object files (ROF), roughly easiest first:

1. **`rof2elf`**: convert Microware `.r`/`.l` files to ELF and keep linking with GNU `ld` +
   `elf2mod`. This brings Microware and CD-i libraries into GCC programs.
2. **`elf2rof`**: convert GCC objects to ROF for the Microware linker `l68`, so GCC code can go
   into Microware-linked programs.
3. **A ROF backend in BFD**, making `as`/`ld`/`objdump` speak ROF natively. Cleanest, but the
   most work.
4. **Make GCC emit Microware `r68` assembly** (`psect`/`vsect`, `dc`/`ds`, no `%`). This is the
   only option touching the compiler. It ties builds to Microware's assembler and linker, and
   breaks GNU-syntax inline asm.

## 6. ROF interoperability: `rof2elf` and `elf2rof`

Goal: both directions, `rof2elf` first (most important).

### The ROF format

From the *OS-9 Assembler/Linker User Manual*, chapter 3 ("Relocatable Program Sections"), section
"Relocatable Object File Format".
An ROF has eight sections:
- the header
- external definitions
- object code
- initialized data
- remote initialized data
- debug information
- external references
- local references

- **Header:** sync bytes, type/language, attribute/revision, assembly-valid flag, series,
  date/time, edition, and sizes (static storage, initialized data, code, stack, remote static,
  remote initialized data, debug). It also holds the entry point and uninitialized-trap entry
  offsets, and a NUL-terminated module name.
- **Symbol types:** data/code/equ, initialized/uninitialized, remote/not remote, common.
- **References** (external and local): each one carries type and location flags.
  - Size: 1, 2 or 4 bytes.
  - Relative: PC-relative, relative to the reference's own address.
  - Negative: subtract the symbol's value instead of adding it.
  - Target: code or data, remote or not.
  - There is no addend field; the addend sits in the object bytes (REL style).
- **Libraries** (`.l`) are concatenated ROFs, not `ar` archives.

### Mapping to ELF

| ROF | ELF | Notes |
|---|---|---|
| code | `.text` | one code section per ROF; `.rodata`/`.text.*` get merged into it on the way back |
| initialized / uninitialized data | `.data` / `.bss` | |
| remote data (`vsect remote`) | `.data.remote` / `.bss.remote`, beyond the 64K a6 window | see "Large programs" |
| 2-byte data references (a6-relative) | `R_68K_16` | must agree with the -0x8000 bias in the linker script |
| 4-byte references | `R_68K_32` | in `.data`: pointers, which `elf2mod` relocates at load time. In code: link-time constants, i.e. a6-relative data offsets (`move.l #_iob,a1` then `adda.l a6,a1` in `cstart.r`) or equates, which come out right with the -0x8000 data bias; `elf2mod` accepts these |
| relative 1/2/4 bytes | `R_68K_PC8`/`PC16`/`PC32` | |
| negative references | `R_68K_PC32` when paired | an external reference paired with a negated reference to the same ROF's code is a PC-relative value (`cstart.r`'s `jsr (pc,dN.l)` calls); other negative references are rejected, unless only equates are involved, which `rof2elf` folds in (with `-e sys.l`) |
| common symbols | ELF COMMON | |
| addend in the instruction bytes | RELA addend | |
| mainline header (type, attributes, edition, stack, entry) | absolute and code symbols `__os9_tylan`, `__os9_attrev`, `__os9_edition`, `__os9_stack`, `__os9_entry`, `__os9_trapent` | `elf2mod` doesn't use them yet (§12) |
| debug info | dropped | Microware's format differs from DWARF |

Other things to handle:

- **Long calls:** GNU `ld` doesn't build Microware's `_jmptbl` jump table for calls beyond ±32K;
  `elf2mod` will (see "Large programs").
- **Linker-defined symbols:** other symbols the Microware libraries expect from their linker
  must come from the linker script. The first link's undefined-symbol errors will show the list.
- **Calling convention:** calls between GCC code and converted Microware code
  need `-mos9call` (§4).
- **Constructors:** C++ static constructors need their own startup mechanism in the ELF→ROF
  (`elf2rof`) direction, since `l68` has nothing like `.init_array`. ROF has no weak symbols.

### Prior art: the old binutils ROF backend

The GCC 2.6.2 OS-9 cross port bundles a Cygnus binutils 2.5.2 (1995) with a BFD target for ROF:
`bfd/roff.c` (about 3,400 lines, "hacked from adobe-aout.c by Walter Hunt") plus `bfd/os9.c`.

What it does:
- Reads and writes ROF objects.
- Presents `.l` files as archives.
- Handles 1/2/4-byte, PC-relative, negative and `equ` references.
- Applies the 0x8000 a6 bias to data references.
- Maps onto `.text`/`.data`/`.bss`/`.debug`.

Its gaps:
- It aborts on remote data and OS-9 common symbols.
- Its relocation types are ROF codes rather than m68k ones. Converting with `objcopy` would rely
  on modern BFD's generic remapping by size and PC-relative flag, which can't represent negative
  references or the a6 bias.

### Decision: standalone tools

Porting `roff.c` into binutils 2.35 or newer would mean rewriting nearly all of its BFD glue:
- `DEFUN`/`PROTO` macros
- `bfd_read` → `bfd_bread`
- section size fields
- target jump tables
- archive interfaces
- the removed "seclet" linker

Mixed-format ELF links would also stay fragile. Instead:

- **`rof2elf`** (first): parses ROF, splits `.l` files, and writes ELF relocatables through
  libbfd (like `elf2mod`) or directly, with exactly the relocations we choose. Vendor libraries
  are converted once into `.a` archives; your own Microware objects through a Makefile rule
  (`%.o: %.r`). A wrapper or GCC driver spec could hide the step later. Estimated 600–900 lines.
- **`elf2rof`** (later): reads an ELF `.o` (or a `ld -r` result) and writes ROF for `l68`.
  Similar size.
- **Specification:** the Assembler/Linker manual, plus `roff.c` for real-world details.
- **Location:** both live in a fork of `elf2mod` (`cdifan/elf2mod`), next to `elf2mod` itself. All
  three tools handle OS-9 module headers, CRCs and ELF via libbfd, so they can share code, and they
  build in the existing `elf2mod` step of the `Dockerfile`. binutils itself stays unchanged.
- **A BFD port** remains possible later, if `nm`/`objdump` on ROF or direct `ld` input turn out
  to matter.

### Large programs

On the 68000 (and the CD-i's 68070), both code and data are normally reached with 16-bit
displacements: calls and PC-relative references reach ±32 KB, and a6-relative data reaches the
64 KB around a6 (which points 0x8000 into the data area). Larger programs need two extensions.

#### Code beyond ±32 KB: a jump table, built by `elf2mod`

The Microware linker `l68` solves this with a jump table, `_jmptbl`, in the data area (`l68 -a`,
which `cc` uses by default; *OS-9 Assembler/Linker User Manual*, chapter 6 "The Linker" and
chapter 7 "OS-9 Programming Techniques"). Each entry is a `jmp` to an absolute address (fixed up
when OS-9 loads the module), and each call that's too far is redirected through an entry, with
one entry per unreachable destination. `l68` redirects `bsr.w` and PC-relative `lea`/`pea`;
conditional branches (`Bcc`, `DBcc`) stay limited to ±32K. Our table below adds `bra.w` (tail
calls). `roff.c` did the same in its GNU
linker. We do it in `elf2mod`, after linking, which keeps binutils unchanged:

1. Compile as usual. With `-mbsrw`, calls are `bsr.w`; without it, `-mpcrel` calls are
   `lea f(pc),aN` followed by `jsr (aN)`. Both forms are patchable, so `-mbsrw` isn't required.
2. Link with `-q` (keep relocations, as already required) and `--noinhibit-exec`. ld then still
   reports "relocation truncated to fit" for every call that's too far, but writes the output
   anyway and keeps the relocations. Verified with the toolchain: without the option no output is
   written; with it, ld exits with status 0.
3. `elf2mod` finds the overflowed relocations (`R_68K_PLT16` for most `-mbsrw` calls, `R_68K_PC16`
   for calls to `static` functions in other sections and for PC-relative address loads). It
   identifies each instruction by its opcode word just before the relocation (checked under a mask),
   not by the relocation type, and refuses anything it doesn't recognise. It gives each far target
   one table entry (`jmp f`, `$4EF9` plus a 32-bit address, with a load-time relocation like
   `R_68K_32` in `.data`), and patches the instruction. Each patch keeps the instruction's size, so
   nothing else moves:

| Instruction | Becomes |
|---|---|
| `bsr.w f` (call) | `jsr _jmptbl+x(a6)` |
| `bra.w f` (tail call) | `jmp _jmptbl+x(a6)` |
| `lea f(pc),An` (taking the address) | `movea.l _jmptbl+x+2(a6),An`: loads the entry's absolute address, i.e. the real address of `f`, so function-pointer comparisons stay correct |
| `pea f(pc)` (pushing the address) | `move.l _jmptbl+x+2(a6),-(sp)` |

- **Space for the table** must already exist inside a6's window, because the data layout is fixed
  after linking. `l68` puts the table at the end of the initialized data, i.e. at the end of the
  64 KB a6 area and before the remote data (the *OS-9 C Language User Manual*, chapter 2, shows
  this layout for the `remote` storage class). The linker script reserves `_jmptbl` at the end of
  `.data` too. Since this toolchain puts `.bss` after `.data` (`l68` assigns the uninitialized data
  first), the table ends up between `.data` and `.bss`, still inside the window. `elf2mod` reports
  how many entries are needed; a first link (or a generous default) sets the size, and a wrapper
  or Makefile rule can automate the two passes.
- **The table must stay reachable:** `elf2mod` checks that the end of `_jmptbl` is within a6's
  reach (a6 + 0x7FFF); a large `.data` could push it out.
- **Condition codes:** `pea` doesn't change them, but its replacement `move.l` does. That's
  harmless for GCC's code, but matters for hand-written assembly that relies on them.
- **Leftover overflows are errors.** Because ld exits with status 0 under `--noinhibit-exec`,
  `elf2mod` must fail if any overflowed relocation remains that it can't patch, e.g. a
  PC-relative *read* of a far constant, or a far address load into a data register.
- **Converted Microware objects** call externals with `bsr.w` and rely on this mechanism, so it
  covers them too.

#### Branches within a function

The jump table only covers calls and address loads. Branches *within* a function are limited to
±32 KB, as with Microware C 3.2 (its linker manual: `Bcc` and `DBcc` "are still limited to the
32K restriction"). GCC emits relaxable branches (`jgt`, `jbra`, …), and gas chooses the size:

| Assembled with | A branch beyond ±32 KB becomes | Usable for OS-9 |
|---|---|---|
| a 68020-class CPU | `bgt.l` (32-bit PC-relative) | no: not a 68000/68070 instruction |
| `-mcpu=68000`, no `--pcrel` | an inverted short branch plus `jmp` to an absolute address (`R_68K_32` in `.text`) | no: OS-9 code must be position-independent |
| `-mcpu=68000 --pcrel` (what `gcc -mpcrel` passes) | an error: "value … out of range" | no, but it fails at compile time |

So with this toolchain's flags, a function containing a branch longer than 32 KB fails to compile
(verified). gas could be taught a position-independent long form, but binutils stays unchanged,
so this remains a documented limitation; such functions are very rare. The same applies to
`-freorder-blocks-and-partition`, which moves cold code to `.text.unlikely` and reaches it with
conditional branches: it's disabled by default for this target, and should stay off for large
programs.

**Pitfall, the assembler's default CPU:** run by hand, `m68k-elfos9-as` defaults to a 68020-class
CPU, despite the `--with-cpu=68000` passed to the binutils build. It then silently turns far
relaxable branches into 68020-only `bcc.l` instructions (verified). Through `gcc`, the correct
options (`-mcpu=68000 --pcrel`) are passed, but hand-written assembly assembled directly, like the
README's `cstart.s` example, should use `m68k-elfos9-as -m68000 --pcrel`.

#### Data beyond 64 KB: remote data

Variables that don't fit in the 64 KB a6 window are marked *remote*, like Microware C 3.2's `remote`
storage class (*OS-9 C Language User Manual*, chapter 2; Microware C 3.2 also has `-k0l`, 32-bit
offsets for all data):

- An attribute (`remote`) places them in `.data.remote` / `.bss.remote` sections, which the linker
  script puts after the normal data, beyond the window.
- GCC accesses them with a 32-bit offset in an index register, e.g. `move.l #sym,dN` followed by
  `0(a6,dN.l)`; small data keeps the fast 16-bit form. (The GCC 1.37.1 OS-9 port's `-mremote`
  did the same.)
- `elf2mod` accepts these 32-bit references from code to data as link-time constants (a6-relative
  offsets), not as pointers needing load-time relocation; today it rejects them.
- `rof2elf` maps Microware's remote vsects onto the same sections.

### Debug symbols for Microware's debuggers

Microware's debuggers read two files that the Microware linker writes with `l68 -g` (or `cc -g`).
They're placed in an `STB` directory next to the program if one exists:

- `<program>.stb`, a **symbol module**: global code and data symbols. Used by the user-state
  debugger `debug` and by the source-level debugger SrcDbg.
- `<program>.dbg`, **source-level information**: line numbers, local variables and types. Used by
  SrcDbg. It's only produced when the `.r` files were compiled with `-g`.

SrcDbg can be told to skip either file, so the two can be supported independently.

#### `.stb` symbol module: documented, planned for `elf2mod`

The format is specified in Appendix A of the *OS-9/68000 User-State Debugger* manual, with an
example dump. It's an ordinary OS-9 data module:

| Part | Contents |
|---|---|
| module header | standard 48-byte OS-9 module header (type: data module) |
| name | NUL-terminated module name (`<program>.stb`) |
| STB header | 2-byte STB format number (0x0100), 4-byte CRC of the program module, 4-byte offset to the symbol entries, 4-byte number of entries, padding to the next 16-byte offset |
| symbol entries | sorted by ascending value; each is a 4-byte value, a 2-byte type flag (low 3 bits: 0 uninitialised data, 1 initialised data, 2 remote data, 4 program text, 6 absolute) and a 4-byte name offset |
| names | NUL-terminated symbol names |
| module CRC | as for any OS-9 module |

The program-module CRC lets the debugger check that the symbols match the program. The old
`roff.c` backend wrote this format too, which fills in some real-world details:
- data symbol values are a6-relative, including the 0x8000 bias
- code symbol values are offsets from the start of the module
- some symbols get an extra type bit (0x2000) beyond the documented low 3 bits

**Plan:** generate the `.stb` in `elf2mod`, from the linked ELF's symbol table. `elf2mod` builds
the program module and its CRC anyway. An option would select whether to write it.

#### `.dbg` source-level information: undocumented, research item

None of the available manuals describe the contents of `.dbg` files:
- The Assembler/Linker manual only calls the ROF "Debug Information" section variable-length, and
  says `l68 -g` makes a `.dbg` when the `.r` files contain debug information.
- `roff.c` writes a `.dbg` with a magic number and the program CRC (at offset 6), followed by
  the collected debug sections. According to Hunt's announcement, its debug support was taken
  from Stephan Paschedag's OS-9 port of GCC 2.5.8. So the layout may come from an earlier
  OS-9-hosted GCC rather than being Hunt's own invention, but whether it matches Microware's
  `.dbg` format is still to be checked.

Supporting SrcDbg would mean reverse-engineering the Microware C 3.2 debug format, and translating
GCC's DWARF into it. That's feasible with the original compiler and linker, using the DOS-hosted
cross versions (§7):
1. compile small sources with `xcc -g -a` and study how the compiler emits debug information in
   assembly form
2. compile them to `.r` with `-g` and link with `l68 -g`, then examine the ROF debug sections and
   the resulting `.dbg`
3. vary the source systematically (lines, nesting, locals and parameters, register variables,
   structs, unions, enums, typedefs, arrays, pointers) and compare the outputs

Mapping DWARF line tables, scopes, variable locations and types onto that format is a substantial
project, so it's a research item for later, after `rof2elf` and `.stb` support.

## 7. Microware tools available for testing

> **Copyright notice.** Microware OS-9 and its development tools are proprietary. Microware LP,
> which says it owns all OS-9 copyrights and trademarks, enforces this. On 5 August 2026 it filed
> a [DMCA notice][microware-dmca] that got a public GitHub copy of the OS-9/68K SDK blocked. This
> project therefore does not link to, copy or redistribute any Microware tools, libraries or
> headers. The tools below are used only locally, by developers who have them, to observe their
> behaviour. Everything in this project is built from the public manuals and from that
> observation.

[microware-dmca]: https://github.com/github/dmca/blob/master/2026/08/2026-08-05-microware.md

- **Microware OS-9/68K 3.2.4 SDK (MWOS tree)**, with Windows-hosted tools in `DOS/BIN`:
  - compilers and assembly: `xcc` (Microware Ultra C 2.5), `r68`, `l68`, `rdump`, `deasm`
  - `os9*` disk utilities
  - documentation in `DOC/PDF`, including the compiler manuals *Using Ultra C/C++*
    (`ultrac_use.pdf`), the *Ultra C/C++ Processor Guide* (`ultrac_pg.pdf`, with a 68K ABI
    section on register usage, argument passing and struct return) and the *Ultra C Library
    Reference* (`ultrac_lib_ref.pdf`), plus `DOS/BIN/compiler.chm`
  - CD-i variants of the tree also carry the CD-i headers and libraries from Microware C 3.2,
    the older OS-9 C compiler (`clib.l`, `cdisys.l`, `cstart.r`, `math.l`, …)

  To run Ultra C 2.5's `xcc` from a POSIX-style shell (Git Bash, MSYS2) on Windows:

  ```sh
  export MWOS='<path to the MWOS tree, Windows syntax>'
  export PATH="<MWOS tree>/DOS/BIN:$PATH"
  MSYS2_ARG_CONV_EXCL='*' xcc.exe -mode=compat -to=osk -tp=68k -a -c -s file.c   # → file.a
  ```

  Gotchas:
  - Without `DOS/BIN` on `PATH`, phase launches fail with "argument list length error while
    forking".
  - `-tp=68000` and `-tp=070` are rejected; use `-tp=68k`.
  - Compat mode uses the old `cc` option set (`-a` for assembly, `-bp` to show phases).
  - Set `MWOS` rather than using `-mw=`.
  - Float code comes out with 68881 instructions; ignore it when probing argument placement.
- **Microware C 3.2**, the original compiler, in two forms:
  - **DOS-hosted cross compiler.** The `xcc` driver runs the phases `cpp`, `c68` (compiler), `o68`
    (optimizer), `r68` (assembler) and `l68` (linker); headers and libraries are found through the
    `CDEF`/`CLIB` environment variables. It runs in a DOS emulator such as vDos, DOSBox or MS-DOS
    Player. Same options as native `cc`: `xcc -a -s file.c` → `file.a`.
  - **Native `cc`** inside CD-i Emulator (§9), from a Microware disk image.

  Either one confirms the §3 results with the original compiler.
- **Cross-checking compiled libraries:** disassemble a few functions from the CD-i `clib.l` with
  `rdump`/`deasm` to confirm the conventions independently.

## 8. Build environment

GCC cannot be built with MSVC (Visual Studio). It needs a POSIX shell environment (configure,
GNU make, sed/awk, m4, flex/bison) and a GCC-compatible host compiler. Git for Windows' Git Bash
is a cut-down MSYS2 runtime with neither a compiler nor a package manager.

Options:

1. **WSL2 + Ubuntu (chosen).** Same environment as the `Dockerfile`, and the fastest builds.
   Windows tools such as `xcc.exe` can be called from inside WSL for compatibility testing. Keep
   the build directory inside the Linux filesystem (e.g. `~/build`), not under `/mnt/c`, which is
   slow for builds.
2. **MSYS2 + mingw-w64 GCC.** Produces native Windows `.exe` tools. Builds are roughly 3–5×
   slower, with occasional Windows-specific build fixes.
3. **Cygwin.** Slowest of the three; not recommended.

Estimates on a 4-core machine under WSL2:
- `cc1` + `libgcc` only: ~10–20 minutes
- full toolchain: ~30–45 minutes
- incremental backend rebuilds: a minute or two
- disk: under ~15 GB including the distribution

Setup plan: install WSL2 with Ubuntu, create a sudo-capable user, install the `apt` dependencies
listed in the `Dockerfile`, then configure GCC as in the `Dockerfile`
(`--target=m68k-elfos9 --with-cpu=68000 --enable-languages=c …`).

## 9. Emulator serial bridge

Optional, but generally useful: a way for tools (and Claude) to send commands to an OS-9 shell
running inside an emulator, such as [CD-i Emulator][cdiemu] (`cdiemu`) by CD-i Fan, and read its
output through one of the emulated serial ports. `cdiemu` emulates CD-i hardware with OS-9
running on it, and can run Microware tools such as native `cc` from a Microware disk image.
The Windows version (`wcdiemu`) can connect each serial port to its terminal program
(`winterm`) via Windows messages:

[cdiemu]: https://www.cdiemu.org

| Message | Value | Use |
|---|---|---|
| `WM_TERMHOST` | `WM_USER + 0x100` | host → terminal: `wParam` = host id, `lParam` = host window |
| `WM_TERMDATA` | `WM_USER + 0x101` | either way: `lParam` = `MAKELONG(char, TERMDATA_*)` |
| `WM_TERMWND` | `WM_USER + 0x102` | terminal → emulator window: late binding (see below) |
| `WM_COPYDATA` | — | strings, `dwData = TERMDATA_CHAR` |

`TERMDATA_*` codes: `NULL`, `CHAR`, `EXIT`, `BREAK`, `ERROR`, `SIGNAL`, `TITLE`.

Binding works two ways:
- **Early:** the emulator does `FindWindow("TermWndClass", title)`, or else launches
  `winterm.exe /title …`.
- **Late:** a terminal sends `WM_TERMWND` to the emulator window (class `CdiWndClass`), which
  matches it to a serial port by window title.

A bridge would be a hidden `TermWndClass` window with a fixed title, plus a command-line client
that sends a command line and collects output until the OS-9 shell prompt returns. It could be
written in C++ (Visual Studio) next to `winterm`, or in PowerShell/C#.

Limitations:
- `cdiemu` cannot yet mount a host directory as an OS-9 device (planned).
- Files must travel over the serial line, and binary files such as `.r` need hex encoding.

For compiler testing, the Windows-hosted Microware cross tools (§7) are the easier path. The
bridge matters for running programs inside the emulator.

## 10. Licensing and upstreaming

### Fork patches: authorship and license

All fork commits in every submodule are authored and committed by Murachue (2021). The upstream
split points, fetched shallowly and tagged locally in each submodule:

| Submodule | Upstream split | Fork changes |
|---|---|---|
| gcc | `releases/gcc-11.1.0` = `50bc918` (tag `gcc-11.1.0-base`) | 9 commits, 7 files, +89/−15 |
| binutils-gdb | the pinned commit `2cb5c79` *is* upstream "2.35 Release" | none |
| newlib-cygwin | `415fdd4` newlib 4.1.0 (tag `newlib-4.1.0-base`) | 2 commits, `newlib/configure.host`, +5/−1 |
| psximager | `9c32ba2` of cebix/psximager (tag `cebix-base`) | 2 commits, `src/psxbuild.cpp`, +36/−13 |
| elf2mod | no upstream (Murachue's own; README says MIT) | — |

There is no explicit license statement or `Signed-off-by:` on the patches. The only explicit
license in this repo is MIT for the `Dockerfile`, and that header excludes the built image. But
the GCC patches modify files under GPL version 3 or later, and publishing modified GPL code means
it is distributed under the GPL. So they are effectively GPLv3+, and newlib's patches fall under
newlib's licenses.

### GCC's AI policy

The [GCC AI policy][gcc-ai-policy], adopted 29 July 2026 and to be reviewed at the start of 2027:

[gcc-ai-policy]: https://gcc.gnu.org/ai-policy.html

- GCC declines legally significant contributions (more than roughly 15 lines) that include or
  are derived from LLM-generated content.
- Small LLM-generated contributions may be accepted if clearly marked.
- **Test cases are an exception:** they may be LLM-generated even when large.
- Commits containing LLM-generated content need an `Assisted-by:` tag.
- Only a human may give the DCO `Signed-off-by:`, and an LLM may not commit.
- Using AI for research, analysis, bug finding, review and debugging is fine, as long as its
  output doesn't end up in the contribution.

"Legally significant" applies cumulatively. The [GNU maintainers' guide][gnu-legally-significant]
notes that a series of small changes by the same person can add up to a significant contribution,
so splitting LLM-written code into sub-15-line patches doesn't change its status.

[gnu-legally-significant]: https://www.gnu.org/prep/maintain/html_node/Legally-Significant.html

Murachue's 2021 patches are human-written and unaffected. Upstreaming them would still need:
- ChangeLog entries and `invoke.texi` documentation
- a proper header for `m68k-os9.h`
- a real fix in place of the "HACK" commit
- removing `dbxelf.h`
- checking that the current GNU `config.sub` still accepts `elfos9` (the pinned copies do)
- a maintainer for the target, and posted testsuite results
- preferably Murachue's own DCO sign-off; they can be contacted via GitHub

### Decision

**The fork's own additions stay fork-only and uncontributable for now.** `-mos9call` and similar
compiler changes will be written with AI assistance, so they can't go into GCC mainline under
the current policy. Standalone tools (`rof2elf`, `elf2rof`, …) aren't affected by GCC's policy.

### Repositories and branches

Development happens in forks under [github.com/cdifan][cdifan]:

[cdifan]: https://github.com/cdifan

- **`cdifan/toolchaincdi`** (forked from `murachue/toolchaincdi`): the superproject. CI publishes
  `ghcr.io/cdifan/toolchaincdi:latest` on every push to `main`. So `main` only receives states
  worth publishing, and work in progress goes on a long-lived **`compat-dev`** branch, merged into
  `main` when ready. Image-neutral changes (docs, CI fixes) may go to `main` directly.
- **`cdifan/gcc`** (forked from `gcc-mirror/gcc`, so upstream branches and tags are available):
  Murachue's **`11.1.0-os9`** branch is kept unchanged as the reference. Our work goes on
  **`11.1.0-os9-compat`**, branched from it, and `.gitmodules` will point `src/gcc` at that
  branch once it has commits.
- Other submodules (newlib, elf2mod, …) are forked when they need changes, and follow the same
  pattern: the upstream branch is kept, and the work branch gets a `-compat` suffix.
- **`cdifan/elf2mod`** (to be forked from `murachue/elf2mod`, branch `main-compat`) will also hold
  `rof2elf` and `elf2rof` (§6), and the `.stb` generation.

Status: `cdifan/toolchaincdi` and `cdifan/gcc` exist, and Murachue's `11.1.0-os9` has been pushed
to `cdifan/gcc`; `cdifan/elf2mod` is still to be forked. The
`compat-dev` and `11.1.0-os9-compat` branches haven't been created yet.

### Alternative (not chosen): human-written code with AI guidance

If upstream inclusion becomes a goal, a human could write the compiler code while AI assistance
stays within what the policy allows. That would mean:
- design specs: the rules and invariants, as in §3–§4
- naming insertion points, and explaining GCC internals
- reviewing the human's patches and debugging failures, describing problems rather than
  supplying replacement code
- writing the test cases, which the policy allows outright

Detailed pseudocode that gets transcribed line by line should be avoided, because the result
would arguably be "derived from LLM-generated content". When in doubt, ask on the
gcc mailing list first.

The insertion points and the implementation order are described in §4 ("Implementation points").

## 11. Test plan

The test harness is set up **before** implementation starts, and each implementation phase of
`-mos9call` (§4, "Implementation points") is gated by the levels below. Test cases may be written
with AI assistance; GCC's AI policy explicitly allows LLM-generated test cases (§10).

The reference for expected behaviour is the Microware C 3.2 output
([`test/os9c/testos9c-cc32.a`](test/os9c/testos9c-cc32.a)) and the rules derived from it in §3.
The exception is struct return, which follows Ultra C 2.5
([`test/os9c/testos9c-ucc25.a`](test/os9c/testos9c-ucc25.a)), as decided in §4.

### Level 0: no-regression baseline

Without `-mos9call`, the compiler must behave exactly as before.

- Before any compiler change, compile a fixed corpus with the unmodified fork and keep the
  generated assembly as the baseline. The corpus is the probe `test/os9c/testos9c.c` plus
  realistic code, such as a selection of newlib and libgcc sources. Compile it at `-O0` and `-O2`,
  with the usual OS-9 flags (`-mpcrel -ma6rel`, with and without `-mbsrw`).
- After every change, recompile the corpus without `-mos9call` and require
  **byte-identical** assembly.
- A small script does the compile-and-diff; it runs in the build environment (§8).

### Level 1: compile-only ABI checks

These verify where values go, by scanning the generated assembly. No m68k hardware or simulator is
needed.

- **Framework:** GCC's own DejaGnu testsuite, as `gcc/testsuite/gcc.target/m68k/os9call-*.c` with
  `{ dg-do compile }`, `{ dg-options "-mos9call ..." }` and `{ dg-final { scan-assembler ... } }`.
  Run with `make check-gcc RUNTESTFLAGS="m68k.exp=os9call*"` (requires `dejagnu`). For a cross
  compiler this needs a DejaGnu board file describing the target; for compile-only tests a minimal
  one without a simulator is enough.
- **Marker constants:** call sites pass distinctive values, e.g.
  `f(0x11111111, 0x22222222, 0x33333333)`, so each pattern pins one fact: `#0x11111111` → `%d0`,
  `#0x22222222` → `%d1`, `#0x33333333` pushed. Callees return or store a specific parameter, so
  the pattern shows where the callee reads it from.
- **Coverage:** one test per rule in §3:
  - argument positions, including `int, double, int`, `struct, int` and double first
  - narrow, pointer and float arguments
  - indirect calls
  - variadic calls placed like unprototyped calls: `g(int, ...)`, `h(int, int, ...)` and, in C++,
    `f(...)`, with promotions of unnamed `float`/`char`/`short` arguments
  - an error when a variadic function is *defined* under `-mos9call` (until the callee side exists)
  - return values
  - which registers the prologue saves (d1 unless it's an argument or return register; a0/a1;
    never d0)
  - struct return in the a0 convention (buffer address in a0, all aggregates)
  - no sibling call from os9call functions
  - the scope rules of §4: member functions (`METHOD_TYPE`) and libcalls unchanged, in C and C++
  - static member functions and capture-less lambda conversions use the OS-9 convention (C++)
  - `interrupt_handler` functions excluded; the `stackcall` and `os9call` attributes, including
    through function pointers and typedefs
  - `os9call`-attributed declarations (e.g. via `_OS9PROTO`) are called with the OS-9 convention
    when `-mos9call` is off, and `__OS9CALL__` is defined only with the flag
  - assigning between function pointers of different effective conventions is diagnosed as
    incompatible; an unattributed pointer under `-mos9call` and an `os9call` pointer are compatible
  - `long long` in d0:d1; `long double` and `_Complex` on the stack and returned via a0
  - small structs and unions with integer modes still passed on the stack
  - odd-sized structs (1, 2, 3, 5, 6, 7 bytes) in 4-byte-rounded slots, data at the start of the
    slot
  - data layout unchanged by `-mos9call` (member offsets, sizes, `enum` size, bitfield order as in
    probe tests 70–79)
  - float returned in d0; with `-m68881`, return values still in d0 / d0:d1 and FPU registers saved
  - the option rules: `-mshort`, `-mrtd`, `-pg`, `-finstrument-functions`, `-fprofile-arcs`,
    `-fprofile-generate`, `-fuse-cxa-atexit` and (with `-m68020`) `-fstack-limit-symbol` and
    `-fstack-limit-register=d0`/`d1` rejected; warnings for `-m68881`, missing `-ma6rel`,
    `-malign-int`, `-fshort-enums` and `-fpack-struct`
  - functions with frames of 32 KB or more keep a1 intact
  - C++ mangling: the redundant attribute is dropped, so `U9stackcall` appears only under
    `-mos9call` and `U7os9call` only without it; unattributed types mangle as with the regular
    compiler
  - runtime functions called through built-in declarations use the stack convention under
    `-mos9call`: the `memcpy` call from a large struct copy; explicit calls to `strlen`, `malloc`
    and `free`; `_Unwind_Resume`, in C with `-fexceptions` and a `cleanup` variable (and in C++
    once enabled)
  - user redeclarations without a convention attribute (K&R `char *malloc();`, a private
    `memcpy` prototype) still call with the stack convention
  - nested functions: direct calls accepted, an error when a trampoline would be needed
  - `__builtin_apply` and friends rejected
  - `main`, `qsort` comparators and signal handlers compiled with the OS-9 convention

### Level 2: execution-level ABI checks

Level 1 can't prove that registers actually survive a call, or that values arrive intact. Level 2
runs code.

- **Harness:** a small host program built around the Musashi 68000 CPU emulator library
  (permissively licensed, included as a submodule). It loads a linked test image (ELF, no OS), sets
  up a stack and a6 (for `-ma6rel` code), fills all other registers with marker values, calls a
  function, and checks the results.
- **Microware side:** short hand-written GNU-syntax assembly stubs reproduce the exact call and
  return sequences of Microware C 3.2 (taken from `testos9c-cc32.a`), and for struct return those
  of Ultra C 2.5 (from `testos9c-ucc25.a`). They let the harness check:
  - a Microware C 3.2-style caller calling a GCC `-mos9call` function: arguments received correctly,
    and d1 (when not an argument/return register), a0/a1 and d2–d7 preserved
  - a GCC `-mos9call` caller calling a Microware C 3.2-style callee: arguments placed correctly, and
    no reliance on d0 surviving
  - doubles in d0:d1, the stack layout, and caller cleanup
  - struct return in the a0 convention
  - once the callee side of variadic functions exists: a GCC variadic function called
    Microware C 3.2-style (first arguments in d0/d1), reading every argument correctly with `va_arg`
- **GCC↔GCC:** `-mos9call` code calling itself, including through function pointers, mixed with
  non-os9call code (libcalls, member functions).
- This level gates implementation phases 3 (callee register saves) and 4 (return values).

### Level 3: real interoperability

The final proof uses real Microware objects. It depends on `rof2elf` (§6), and ideally the serial
bridge (§9) for automation.

- Compile test modules with Microware C 3.2 (and Ultra C 2.5) to `.r`, convert them with `rof2elf`,
  link them with GCC `-mos9call` objects, convert with `elf2mod`, and run the result in CD-i
  Emulator (§9). Calls go both ways.
- Also link against the CD-i libraries (`clib.l`, `cdisys.l`) and call real library functions.
- **Done so far, without OS-9:** `make check-mw MWLIB=…` in `test/abi-exec` converts the local
  `cstart.r`, `clib.l` and `sys.l` with `rof2elf` and runs `mwtests/` in the Level 2 emulator. The
  test calls `strlen`, `atoi`, `strcmp`, `strcpy`, `strcat`, `sprintf`, `sscanf`, `toupper`,
  `index` and `atol`; Microware's `qsort` calls back a GCC comparator; and GCC's own `memcpy`
  for a struct copy goes to `clib`'s (with `-mbuiltin=os9call`). Functions that make system calls
  (memory, I/O, floating point through the math trap) need the real OS-9 in CD-i Emulator.
- **`rof2elf`/`elf2rof` round trips:** convert ROF → ELF → ROF and compare with `rdump`. Convert
  ELF → ROF and link with `l68`.

### Mapping to implementation phases

| `-mos9call` phase (§4) | Gated by |
|---|---|
| 1. option, attributes, state, init | Levels 0 and 1 (byte-identical without the flag; attribute, compatibility and option tests) |
| 2. argument placement | Levels 0 and 1 |
| 3. callee register saves | Levels 0, 1 and 2 |
| 4. return values | Levels 0, 1 and 2 |
| 5. no tail calls | Levels 0 and 1 |
| later: real use with Microware libraries | Level 3 |

### Where tests live

Tests live under `test/` in this repository, outside `src/`: the `Dockerfile` copies `src/` into
the build, and tests don't belong in the image.

```
test/
├── os9c/          the Microware C probe and its Microware C 3.2 / Ultra C 2.5 outputs
├── baseline/      Level 0: the corpus list and the compile-and-diff script
└── abi-exec/      Level 2: emulator harness, assembly stubs and tests
    ├── mwtests/   Level 3 (partial): tests linked with Microware's converted C library
    └── musashi/   68000 emulator library (submodule)
```

- DejaGnu tests (Level 1): in the GCC tree, on the `11.1.0-os9-compat` branch of `cdifan/gcc`,
  where GCC's test framework expects them.
- Level 0 baselines aren't committed: the script compiles the corpus with the unmodified and
  the changed compiler, and diffs the two. Committed baselines would go stale with every
  compiler build.
- Microware tools and libraries are used only locally and are never committed (§7).

## 12. Open questions and next steps

1. **Build environment:** done: WSL2 + Ubuntu (§8), with the unmodified fork as the Level 0
   reference compiler (§11).
2. **Test harness:** done: Level 0 (`test/baseline`), Level 1 (DejaGnu tests in the GCC tree,
   board in `test/dejagnu`), Level 2 (`test/abi-exec`) and a partial Level 3 (§11).
3. **`-mos9call`:** done on GCC 11.1, phase by phase (branch `11.1.0-os9-compat` of
   `cdifan/gcc`), plus the fixes from an implementation review and `-mbuiltin=stackcall|os9call`
   (§4). Still possible: a cross-check against a disassembly of `clib.l`.
4. **`rof2elf`:** done, in `cdifan/elf2mod` (`rof2elf.md` there). It converts the CD-i libraries
   and `cstart.r`; 4-byte references to data and equates are link-time constants, which `elf2mod`
   now accepts in code; the mainline header becomes `__os9_*` symbols; the linker script must
   define `end`. Still open: remote data in `elf2mod` and the linker script, and using the
   `__os9_*` symbols in `elf2mod`'s module header. Level 3 runs partly (§11); Microware C 3.2's
   variadic functions were confirmed by running them (§4).
5. **`elf2rof`**, then the deferred items:
   - porting the fork to GCC 17 (§2)
   - enabling C++ in the toolchain build (`--enable-languages=c,c++`); the C++ parts of
     implementation phase 1 (mangling) and the C++ Level 1 tests apply once it's enabled
   - the callee side of variadic functions under `-mos9call` (a target-specific `va_list`, §4)
   - building newlib/libstdc++ with `-mos9call`, after the variadic callee side, with `stackcall`
     on the functions listed in §4 "Libraries" (including `setjmp`/`longjmp` and the unwinder
     interfaces)
   - caller-side use of preserved a0/a1
   - the emulator serial bridge
   - `.stb` symbol modules from `elf2mod` (§6)
   - large programs (§6): the `_jmptbl` jump table in `elf2mod`, and the `remote` attribute in
     GCC plus its handling in `elf2mod` and the linker script
   - Microware-compatible stack checking, `-mos9stkchk` (§4, "Stack checking"); requires
     `rof2elf`, and the rest of `_stkcheck`'s contract. Its Level 1 tests: `N` values, placement
     at the start of the prologue, the register saves around the check, the `-ma6rel`
     requirement, and no check in `interrupt_handler` functions
   - the assembler's default CPU (§6, "Branches within a function"): the README now documents
     `-m68000 --pcrel` for hand-written assembly; making the binutils build default to the 68000
     remains an option
   - research: the Microware C 3.2 `.dbg` format for SrcDbg (§6)
   - an upstream latent bug: an `interrupt_handler` function with an FPU and a frame of 32 KB or
     more restores its FPU registers through a1 after restoring a1. OS-9 convention functions
     avoid it (FPU registers first, a1 last); interrupt handlers are left unchanged to keep code
     built without the flag identical
   - libgcc's soft-float routines (`lb1sf68.S`, assembled as PIC because of `-mpcrel`) read their
     rounding mode `_fpCCR` through the GOT, and in a static link with the README's linker script
     the GOT reference resolves to the wrong word (found by the Level 2 harness; independent of
     `-mos9call`). Check how `elf2mod` handles GOT references, and whether libgcc's `.S` files
     should be built without PIC

## 13. References

- Microware OS-9 manuals, hosted by ICDIA: <https://www.icdia.co.uk/microware/index.html>
  - *OS-9 C Language User Manual*: <https://www.icdia.co.uk/microware/77165104.pdf>
    (chapter 2 "Compiler Implementation", chapter 3 "Compiler Organization")
  - *OS-9 Assembler/Linker User Manual*: <https://www.icdia.co.uk/microware/77165106.pdf>
    (chapter 3 "Relocatable Program Sections", section "Relocatable Object File Format";
    chapter 6 "The Linker"; chapter 7 "OS-9 Programming Techniques")
  - *OS-9 C Compiler Version 3.2 Release Notes*: <https://www.icdia.co.uk/microware/os9ccmp.pdf>
  - *OS-9/68000 User-State Debugger* (Appendix A: symbol table module format):
    <https://www.icdia.co.uk/microware/userdbg.pdf>
- Microware LP's DMCA notice against a public copy of the OS-9/68K SDK (5 August 2026):
  <https://github.com/github/dmca/blob/master/2026/08/2026-08-05-microware.md>
- GCC mainline mirror: <https://github.com/gcc-mirror/gcc> (base tag `releases/gcc-11.1.0`)
- GCC AI policy: <https://gcc.gnu.org/ai-policy.html>; GNU "legally significant" changes:
  <https://www.gnu.org/prep/maintain/html_node/Legally-Significant.html>
- CD-i Emulator, by CD-i Fan: <https://www.cdiemu.org>
- Andreas Bischoff's OS-9 GCC cross-compiler pages (original host offline; 2001 Wayback snapshots):
  <https://web.archive.org/web/20010428184208/http://prt.fernuni-hagen.de:80/~bischoff/os9cross_e.html>,
  <https://web.archive.org/web/20010526164545/http://prt.fernuni-hagen.de:80/~bischoff/os9cross/hunt.html>,
  <https://web.archive.org/web/20010525130326/http://prt.fernuni-hagen.de:80/~bischoff/os9cross/howto_e.txt>,
  <https://web.archive.org/web/20010524172023/http://prt.fernuni-hagen.de:80/~bischoff/os9cross/gcccross.faq>
- This toolchain's GCC fork: <https://github.com/cdifan/gcc> (branch `11.1.0-os9` is Murachue's);
  Murachue's original repositories: <https://github.com/murachue/gcc>,
  <https://github.com/murachue/binutils-gdb>, <https://github.com/murachue/newlib-cygwin>,
  <https://github.com/murachue/elf2mod>
- GCC 1.37.1 OS-9 port by T. Shinohara and A. Seyama, on the "CDISC 4 Freeware" CD-i developer
  disc (April 1993), whose image is hosted by ICDIA: <https://www.icdia.co.uk/sw_disc/index.html>
- Walter Hunt's OS-9 GCC 2.6.2 cross compiler with ROF-capable binutils 2.5.2,
  `gcc-sun4-os9-2.6.2.src.tar.gz` (11,133,441 bytes, SHA-256
  `7b93f5df50d1017c0aead26e06e43769dc4f7cf6cb9502db57369d897aa5872e`):
  - mirror: <https://www.cdiemu.org/download/gcc-sun4-os9-2.6.2.src.tar.gz> (not linked from the
    download page)
  - original, now offline:
    `ftp://os9archive.rtsi.com/OS9/OSK/GCC/SRC/gcc-sun4-os9-2.6.2.src.tar.gz`. The FTP archive
    appears to have been retired around 2015, and the host now serves only RTSI's company site.
  - discussion: [CDinteractive forum thread][cdinteractive-os9-gcc]

[cdinteractive-os9-gcc]: http://www.cdinteractive.co.uk/forums/cdinteractive/viewtopic.php?t=2629
