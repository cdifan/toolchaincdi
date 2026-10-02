# Level 2 ABI tests: build the abirun harness (Musashi) and run the test
# images.  Run inside WSL/Linux.  See OS9-COMPAT-DESIGN.md, section 11.
#
#   make                 build everything and run the tests (-O2)
#   make check           run the tests at -O0, -O2, -Os and -O3, and with
#                        -m68020 -m68881 on an emulated 68040
#   make check-mw MWLIB=DIR  also link with Microware's C library (see below)
#   make check-mod       run the tests as OS-9 modules made by elf2mod (see below)
#   make GCC=...         compiler driver to test
#   make BUILD=...       build directory (default ~/build/abi-exec)

HERE    := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD   ?= $(HOME)/build/abi-exec
GB      ?= $(HOME)/build/gcc-build
GCC     ?= $(GB)/gcc/xgcc -B$(GB)/gcc/
TPREFIX ?= m68k-elfos9-
HOSTCC  ?= gcc
CFLAGS_T = -O2 -mpcrel -ma6rel -mos9call
ABIRUN_FLAGS =
MUSASHI  = $(HERE)musashi
TESTS    = $(basename $(notdir $(wildcard $(HERE)tests/*.c)))

all: run

# Musashi, with CR/LF line endings removed (its generator expects LF).
MSRC = m68kcpu.c m68kdasm.c m68k_in.c m68kmake.c m68k.h m68kconf.h \
       m68kcpu.h m68kfpu.c m68kmmu.h softfloat/softfloat.c \
       softfloat/softfloat.h softfloat/softfloat-macros \
       softfloat/softfloat-specialize softfloat/mamesf.h softfloat/milieu.h
$(BUILD)/m/.copied: $(addprefix $(MUSASHI)/,$(MSRC))
	mkdir -p $(BUILD)/m/softfloat
	for f in $(MSRC); do tr -d '\r' < $(MUSASHI)/$$f > $(BUILD)/m/$$f; done
	touch $@

$(BUILD)/m/m68kops.c: $(BUILD)/m/.copied
	cd $(BUILD)/m && $(HOSTCC) -o m68kmake m68kmake.c && ./m68kmake . m68k_in.c > /dev/null

$(BUILD)/abirun: $(HERE)harness.c $(BUILD)/m/m68kops.c
	$(HOSTCC) -O2 -w -I$(BUILD)/m -o $@ $(HERE)harness.c \
	  $(BUILD)/m/m68kcpu.c $(BUILD)/m/m68kops.c $(BUILD)/m/m68kdasm.c \
	  $(BUILD)/m/softfloat/softfloat.c -lm

# Test images: crt0, support (memcpy, memset), the test's C file and
# its assembly stubs (tests/NAME.S).
# tests/NAME.flags, if present, replaces the compiler flags.
$(BUILD)/%.elf: $(HERE)tests/%.c $(HERE)tests/%.S $(HERE)crt0.S $(HERE)support.S $(HERE)abi.lds
	mkdir -p $(BUILD)
	$(GCC) -c $(or $(file <$(HERE)tests/$*.flags),$(CFLAGS_T)) -o $(BUILD)/$*.o $(HERE)tests/$*.c
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/$*-stubs.o $(HERE)tests/$*.S
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/crt0.o $(HERE)crt0.S
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/support.o $(HERE)support.S
	$(TPREFIX)ld --no-check-sections -T $(HERE)abi.lds -o $@ $(BUILD)/crt0.o $(BUILD)/$*.o \
	  $(BUILD)/$*-stubs.o $(BUILD)/support.o $(LIBGCC)

LIBGCC = $(shell $(GCC) -print-libgcc-file-name)

run: $(BUILD)/abirun $(TESTS:%=$(BUILD)/%.elf)
	@fail=0; for t in $(TESTS); do \
	  if $(BUILD)/abirun $(ABIRUN_FLAGS) $(BUILD)/$$t.elf; then echo "PASS: $$t"; \
	  else echo "FAIL: $$t"; fail=1; fi; done; exit $$fail

check:
	@for o in O0 O2 Os O3; do echo "== -$$o"; \
	  $(MAKE) --no-print-directory BUILD=$(BUILD)/$$o \
	    CFLAGS_T="-$$o -mpcrel -ma6rel -mos9call" run || exit 1; done
	@echo "== -O2 -m68020 -m68881 (68040)"; \
	  $(MAKE) --no-print-directory BUILD=$(BUILD)/fpu ABIRUN_FLAGS="-c 68040" \
	    CFLAGS_T="-O2 -m68020 -m68881 -mpcrel -ma6rel -mos9call -w" run

# Level 3 (partial): tests linked with Microware's C library (cstart.r,
# clib.l), converted with rof2elf from src/elf2mod.  Microware's files
# are proprietary and not included: point MWLIB at a directory holding
# cstart.r, clib.l and sys.l (lower or upper case).
#   make check-mw MWLIB=/path/to/LIB
MWLIB    ?=
CFLAGS_MW = -O2 -mpcrel -ma6rel -mos9call -mbuiltin=os9call -w
MWTESTS   = $(basename $(notdir $(wildcard $(HERE)mwtests/*.c)))
mwfile    = $(firstword $(wildcard $(MWLIB)/$(1) $(MWLIB)/$(shell echo $(1) | tr a-z A-Z)))

$(BUILD)/rof2elf: $(HERE)../../src/elf2mod/rof2elf.c
	mkdir -p $(BUILD)
	$(HOSTCC) -O2 -o $@ $<

$(BUILD)/mw/cstart.o $(BUILD)/mw/clib.a: $(BUILD)/rof2elf
	@[ -n "$(call mwfile,clib.l)" ] || { echo "check-mw: set MWLIB to the directory with cstart.r, clib.l and sys.l"; exit 2; }
	mkdir -p $(BUILD)/mw
	$(BUILD)/rof2elf -e $(call mwfile,sys.l) -o $(BUILD)/mw/cstart.o $(call mwfile,cstart.r)
	$(BUILD)/rof2elf -e $(call mwfile,sys.l) -o $(BUILD)/mw/clib.a $(call mwfile,clib.l)
	$(TPREFIX)ranlib $(BUILD)/mw/clib.a

$(BUILD)/mw/%.elf: $(HERE)mwtests/%.c $(HERE)crt0.S $(HERE)abi.lds $(BUILD)/mw/cstart.o $(BUILD)/mw/clib.a
	$(GCC) -c $(or $(file <$(HERE)mwtests/$*.flags),$(CFLAGS_MW)) -o $(BUILD)/mw/$*.o $<
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/mw/crt0.o $(HERE)crt0.S
	$(TPREFIX)ld --no-check-sections -T $(HERE)abi.lds -o $@ $(BUILD)/mw/crt0.o $(BUILD)/mw/$*.o \
	  $(BUILD)/mw/cstart.o $(BUILD)/mw/clib.a $(LIBGCC)

check-mw: $(BUILD)/abirun $(MWTESTS:%=$(BUILD)/mw/%.elf)
	@fail=0; for t in $(MWTESTS); do \
	  if $(BUILD)/abirun $(BUILD)/mw/$$t.elf; then echo "PASS: mw/$$t"; \
	  else echo "FAIL: mw/$$t"; fail=1; fi; done; exit $$fail

# Modules: the tests linked with -q, converted by elf2mod (from
# src/elf2mod, with the BFD library under BFDPATH) and run as OS-9 modules
# (abirun -m, which checks the header parity and CRC like OS-9; the .stb
# symbol modules from elf2mod -g are checked with abirun -v); plus modtests/, whose far calls go through elf2mod's
# _jmptbl (linked with --noinhibit-exec, which keeps the output despite
# the truncated relocations).
#   make check-mod
BFDPATH  ?= $(HOME)/build/install/x86_64-pc-linux-gnu/m68k-elfos9
JMPTBL    = 60
MODTESTS  = $(basename $(notdir $(wildcard $(HERE)modtests/*.c)))

$(BUILD)/elf2mod: $(HERE)../../src/elf2mod/elf2mod.c
	mkdir -p $(BUILD)
	tr -d '\r' < $< > $(BUILD)/elf2mod.c
	$(HOSTCC) -O2 -w -I$(BFDPATH)/include -o $@ $(BUILD)/elf2mod.c \
	  $(BFDPATH)/lib/libbfd.a -liberty -lz -ldl

$(BUILD)/%.mod: $(BUILD)/%.elf $(BUILD)/elf2mod
	$(TPREFIX)ld -q --no-check-sections -T $(HERE)abi.lds -o $(BUILD)/$*.qelf \
	  $(BUILD)/crt0.o $(BUILD)/$*.o $(BUILD)/$*-stubs.o $(BUILD)/support.o $(LIBGCC)
	$(BUILD)/elf2mod -g $(BUILD)/$*.qelf $@ > /dev/null

$(BUILD)/far/%.mod: $(HERE)modtests/%.c $(HERE)modtests/%.S $(HERE)crt0.S $(HERE)support.S $(HERE)abi.lds $(BUILD)/elf2mod
	mkdir -p $(BUILD)/far
	$(GCC) -c $(CFLAGS_T) -o $(BUILD)/far/$*.o $<
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/far/$*-stubs.o $(HERE)modtests/$*.S
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/far/crt0.o $(HERE)crt0.S
	$(TPREFIX)as -m68000 --pcrel -o $(BUILD)/far/support.o $(HERE)support.S
	$(TPREFIX)ld -q --noinhibit-exec --no-check-sections \
	  --defsym __jmptbl_size=$(JMPTBL) -T $(HERE)abi.lds -o $(BUILD)/far/$*.elf \
	  $(BUILD)/far/crt0.o $(BUILD)/far/$*.o $(BUILD)/far/$*-stubs.o \
	  $(BUILD)/far/support.o $(LIBGCC) 2> $(BUILD)/far/$*.ld.log
	$(BUILD)/elf2mod $(BUILD)/far/$*.elf $@

run-mod: $(BUILD)/abirun $(TESTS:%=$(BUILD)/%.mod) $(MODTESTS:%=$(BUILD)/far/%.mod)
	@fail=0; for t in $(TESTS:%=$(BUILD)/%.mod) $(MODTESTS:%=$(BUILD)/far/%.mod); do \
	  if $(BUILD)/abirun $(ABIRUN_FLAGS) -m $$t && \
	     { [ ! -f $$t.stb ] || $(BUILD)/abirun -v $$t.stb; }; then echo "PASS: $$(basename $$t)"; \
	  else echo "FAIL: $$(basename $$t)"; fail=1; fi; done; exit $$fail

check-mod:
	@for o in O2 O2bsrw; do echo "== modules, -$$o"; \
	  $(MAKE) --no-print-directory BUILD=$(BUILD)/mod-$$o \
	    CFLAGS_T="$$(echo -$$o | sed 's/bsrw/ -mbsrw/') -mpcrel -ma6rel -mos9call" run-mod || exit 1; done

clean:
	rm -rf $(BUILD)

.PHONY: all run run-mod check check-mw check-mod clean
.SECONDARY:
