#!/bin/bash
# Level 0: compile the corpus with the reference and the changed compiler, without
# -mos9call, and require byte-identical assembly. See OS9-COMPAT-DESIGN.md §11.
#
#   REF_GCC  reference driver                default: ~/build/install-fix/bin/m68k-elfos9-gcc
#            Murachue's 11.1.0-os9 with the fix that changes code without
#            -mos9call on purpose (branch 11.1.0-os9-const-pointers: read-only
#            variables holding addresses are a6-relative data); the
#            unmodified compiler (~/build/install-ref) differs in strftime.
#   NEW_GCC  changed driver                  default: ~/build/gcc-build/gcc/xgcc -B~/build/gcc-build/gcc/
#   SRC, GB, NB  source, GCC build and newlib build directories (defaults under ~/build)
#   WORK     output directory                default: ~/build/baseline-work
set -uo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
SRC=${SRC:-$HOME/build/src}
GB=${GB:-$HOME/build/gcc-build}
NB=${NB:-$HOME/build/newlib-build}
REF_GCC=${REF_GCC:-$HOME/build/install-fix/bin/m68k-elfos9-gcc}
NEW_GCC=${NEW_GCC:-"$GB/gcc/xgcc -B$GB/gcc/"}
WORK=${WORK:-$HOME/build/baseline-work}
NL="-isystem $NB/m68k-elfos9/newlib/targ-include -isystem $SRC/newlib-cygwin/newlib/libc/include -DCOMPACT_CTYPE -D__NO_SYSCALLS__ -DHAVE_INIT_FINI"
LG="-fbuilding-libgcc -DIN_GCC -DCROSS_DIRECTORY_STRUCTURE -DIN_LIBGCC2 -Dinhibit_libc -isystem $GB/m68k-elfos9/libgcc/include -I$GB/m68k-elfos9/libgcc -I$GB/gcc -I$SRC/gcc/libgcc -I$SRC/gcc/gcc -I$SRC/gcc/include"
VARIANTS=(
  "O0:-O0 -mpcrel -ma6rel"
  "O2:-O2 -mpcrel -ma6rel"
  "O2bsrw:-O2 -mpcrel -ma6rel -mbsrw"
  "Osfp:-Os -mpcrel -ma6rel -fomit-frame-pointer -ffunction-sections"
  "O2fpu:-O2 -mpcrel -ma6rel -m68020 -m68881 -fno-omit-frame-pointer"
)
rm -rf "$WORK"; mkdir -p "$WORK/i" "$WORK/ref" "$WORK/new"
fail=0; n=0
while IFS="|" read -r name src flags cflags; do
  name=$(echo $name); [[ -z $name || $name == \#* ]] && continue
  src=$(eval echo $src); flags=$(eval echo $flags); cflags=$(eval echo ${cflags:-})
  if ! $REF_GCC -E $flags "$src" -o "$WORK/i/$name.i" 2> "$WORK/i/$name.err"; then
    echo "PREPROCESS FAILED: $name (see $WORK/i/$name.err)"; fail=1; continue
  fi
  for v in "${VARIANTS[@]}"; do
    tag=${v%%:*}; opts=${v#*:}
    for side in ref new; do
      [[ $side == ref ]] && cc=$REF_GCC || cc=$NEW_GCC
      if ! $cc -S -w $opts $cflags "$WORK/i/$name.i" -o "$WORK/$side/$name.$tag.s" 2> "$WORK/$side/$name.$tag.err"; then
        echo "COMPILE FAILED ($side): $name $tag"; fail=1
      fi
    done
    n=$((n+1))
    if ! cmp -s "$WORK/ref/$name.$tag.s" "$WORK/new/$name.$tag.s"; then
      echo "DIFFERS: $name $tag"; fail=1
    fi
  done
done < "$REPO/test/baseline/corpus.txt"
[[ $fail == 0 ]] && echo "Level 0: $n compilations byte-identical" || echo "Level 0: FAILED"
exit $fail
