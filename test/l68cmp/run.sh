#!/bin/sh
# Compare elf2mod with Microware's linker: prog.c compiled and linked by
# Microware (l68 -g), then the same ROFs converted with rof2elf, linked
# with GNU ld in l68's order and converted with elf2mod -g.  The symbol
# modules (.stb) must list the same symbols, and the program modules must
# be the same up to the data relocation table (whose order differs; OS-9
# doesn't mind, but the CRCs then differ).
#
# Run from Git Bash on Windows, with WSL (Ubuntu) holding the toolchain.
# Microware's tools and libraries are proprietary and not included.
# Either let this script build with Ultra C's xcc and l68 (compat mode):
#   MWOS   the Ultra C SDK (DOS\BIN has xcc, l68), e.g. C:\path\to\sdk
#   MWLIB  directory with Microware C 3.2's CSTART.R, CLIB.L and SYS.L
# or compare with what Microware C 3.2's own xcc and l68 made under vDos
# (see vdos.bat; any case of file names):
#   MWBUILT  directory with prog.r, prog, prog.stb and prog.map
#   MWLIB    as above
# and
#   L68CMP_DIR  work directory (default /tmp/l68cmp)
#   test/l68cmp/run.sh
set -e
: "${MWLIB:?set MWLIB to the C 3.2 LIB directory}"
[ -n "$MWBUILT" ] || : "${MWOS:?set MWOS to the Ultra C SDK, or MWBUILT}"
here=$(cd "$(dirname "$0")" && pwd)
work=${L68CMP_DIR:-/tmp/l68cmp}
rm -rf "$work"; mkdir -p "$work"; cd "$work"
X() { MSYS2_ARG_CONV_EXCL='*' "$@"; }
# Copy file $2 (any case) from directory $1 to $3.
fetch() { cp "$(ls "$1"/* | grep -i "/$2\$")" "$3"; }
wslpath() { cygpath -m "$1" | sed 's|^\(.\):|/mnt/\L\1|'; }

for f in cstart.r clib.l sys.l; do fetch "$MWLIB" $f $f; done
if [ -n "$MWBUILT" ]; then
  echo "== Microware C 3.2 (vDos): $MWBUILT"; stb=--stb
  for f in prog.r prog prog.stb prog.map; do fetch "$MWBUILT" $f $f; done
else
  echo "== Ultra C: $MWOS"; stb=--stb=ucc
  export MWOS PATH="$(cygpath -u "$MWOS")/DOS/BIN:$PATH"
  cp "$here/prog.c" .
  X xcc.exe -mode=compat -to=osk -tp=68k -r -s -bp prog.c > xcc.log 2>&1 || { cat xcc.log; exit 1; }
  X l68.exe -g -n=prog -o=prog -m=prog.map cstart.r prog.r -l=clib.l -l=sys.l > l68.log 2>&1 || { cat l68.log; exit 1; }
fi
# The library members in l68's order, and the owner it set.
tr -d '\r' < prog.map | grep -i "psect from file: .*clib\.l\$" | sed "s/^'\(.*\)' psect.*/\1.o/" > members
owner=$(od -A n -t u2 --endian=big -j 8 -N 4 prog | awk '{print $1 "." $2}')

wsl.exe -d Ubuntu --exec bash -c "set -e
  cd '$(wslpath "$work")'
  B=\$HOME/build/abi-exec
  make -s -C '$(wslpath "$here/../abi-exec")' \$B/rof2elf \$B/elf2mod > /dev/null
  export PATH=\$HOME/build/install/bin:\$PATH
  for f in cstart.r prog.r clib.l; do \$B/rof2elf -e sys.l -o \${f%.?}.\${f##*.}o \$f; done
  mkdir mem; cd mem; m68k-elfos9-ar x ../clib.lo \$(cat ../members); cd ..
  m68k-elfos9-ld -q --no-check-sections -T '$(wslpath "$here/mod.lds")' \
    -o g.elf cstart.ro prog.ro \$(sed 's|^|mem/|' members)
  \$B/elf2mod -n prog -u $owner $stb g.elf gprog > /dev/null"

perl "$here/stbdump.pl" prog.stb > mw.txt
perl "$here/stbdump.pl" gprog.stb > g.txt
fail=0
if diff mw.txt g.txt > stb.diff; then echo "PASS: symbol module"
else echo "FAIL: symbol module (see $work/stb.diff)"; fail=1; fi
# The program modules, up to the data relocation table (header M$IRefs).
irefs=$(od -A n -t u4 --endian=big -j 0x44 -N 4 prog | tr -d ' ')
if cmp -n "$irefs" prog gprog; then echo "PASS: program module"
else echo "FAIL: program module"; fail=1; fi
exit $fail
