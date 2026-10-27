#!/bin/bash

#
#  Copyright (C) 2026 Chris Arena, 2026-10-27.
#  Written in collaboration with Claude Opus 5.5 (Anthropic).
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written to re-run the reproducers that
#  FIXES.md describes against other LADR builds, and carries the
#  library's licence so that it can be distributed with it.
#
#  This file is part of the LADR Deduction Library.
#
#  The LADR Deduction Library is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License,
#  version 2.
#
#  The LADR Deduction Library is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the LADR Deduction Library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

# Builds the control and instrumented trees used by run_all.sh.
#
#   usage: NEW_TREE=/path/to/LADR-2026/clone build/build.sh [all|base|new-asan|new-poison|base-asan|base-poison|old-asan|old-poison]
#
# Builds go under crash-regress/build/.  This tree is only read (`git archive`),
# and NEW_TREE only by rsync; neither is modified.
set -u
S=$(cd "$(dirname "$0")/.." && pwd)
OLD=$(cd "$S/.." && pwd)
NEW=${NEW_TREE:-}
J=8
SAN="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -g -O1"
POI="-g -O1 -Wl,--wrap=free_mem $S/wrap/poison_wrap.o"

fresh_old() {   # fresh_old <commit> <dir>
  rm -rf "$2"; mkdir -p "$2"
  git -C "$OLD" archive "$1" | tar -x -C "$2"
  mkdir -p "$2/bin"
  # 04567a4 committed some prebuilt binaries by accident; remove them so
  # nothing stale can be linked or installed.
  rm -f "$2"/provers.src/{prover9,fof-prover9,ladr_to_tptp,tptp_to_ladr,newauto,newsax,autosketches4}
}
fresh_new() {   # fresh_new <dir>
  rm -rf "$1"; mkdir -p "$1"
  rsync -a --exclude .git --exclude '*.o' --exclude '*.a' --exclude '*.d' \
        --exclude .build_mode --exclude '/bin/*' "$NEW/" "$1/"
  ( cd "$1" && make realclean ) > "$1.clean.log" 2>&1
}
build() {       # build <dir> <xflags> <label>
  local t0=$(date +%s)
  if [ -f "$1/README.advanced" ]; then   # LADR-2026: its Makefiles are -j safe
    ( cd "$1" && make all -j$J XFLAGS="$2" ) > "$1.build.log" 2>&1
  else                                   # 2009 Makefiles: install races the link
    ( cd "$1/ladr" && make -j$J lib XFLAGS="$2" && cd .. && make all XFLAGS="$2" ) > "$1.build.log" 2>&1
  fi
  local rc=$?
  echo "$3: make rc=$rc in $(( $(date +%s) - t0 ))s; bin has $(ls "$1/bin" | wc -l) files"
}
verify_san() {  # verify_san <dir> <label>
  local log="$1.build.log"
  local tot=$(grep -aE '^(gcc|g\+\+|cc).* -c ' "$log" | wc -l)
  local ins=$(grep -aE '^(gcc|g\+\+|cc).* -c ' "$log" | grep -c -- '-fsanitize')
  local mem=0 asan=0
  for o in $(ar t "$1/ladr/libladr.a"); do
    mem=$((mem+1))
    ( cd "$1/ladr" && nm "$o" 2>/dev/null | grep -q __asan ) && asan=$((asan+1))
  done
  echo "$2: $ins of $tot compile lines carry -fsanitize; $asan of $mem libladr.a members reference __asan"
}
verify_poi() {  # verify_poi <dir> <label>
  local n=0 w=0
  for b in prover9 mace4 prooftrans; do
    n=$((n+1)); nm "$1/bin/$b" 2>/dev/null | grep -q __wrap_free_mem && w=$((w+1))
  done
  echo "$2: $w of $n binaries contain __wrap_free_mem"
}

which=${1:-all}
case $which in base|old-*) ;; *)
  [ -d "$NEW/ladr" ] || { echo "set NEW_TREE to a LADR-2026 source tree (e.g. a clone of AlgorithmicTruth/Prover9)" >&2; exit 2; } ;;
esac
# the poison wrapper is linked into every *-poison build
gcc -c -g -O1 -o "$S/wrap/poison_wrap.o" "$S/wrap/poison_wrap.c" || exit 1
case $which in
  base|all)       fresh_old 04567a4 "$S/build/base";        build "$S/build/base" "" base ;;&
  new-asan|all)   fresh_new "$S/build/new-asan";            build "$S/build/new-asan" "$SAN" new-asan; verify_san "$S/build/new-asan" new-asan ;;&
  new-poison|all) fresh_new "$S/build/new-poison";          build "$S/build/new-poison" "$POI" new-poison; verify_poi "$S/build/new-poison" new-poison ;;&
  base-asan|all)  fresh_old 04567a4 "$S/build/base-asan";   build "$S/build/base-asan" "$SAN" base-asan; verify_san "$S/build/base-asan" base-asan ;;&
  base-poison|all) fresh_old 04567a4 "$S/build/base-poison"; build "$S/build/base-poison" "$POI" base-poison; verify_poi "$S/build/base-poison" base-poison ;;&
  old-asan|all)   fresh_old HEAD "$S/build/old-asan";       build "$S/build/old-asan" "$SAN" old-asan; verify_san "$S/build/old-asan" old-asan ;;&
  old-poison|all) fresh_old HEAD "$S/build/old-poison";     build "$S/build/old-poison" "$POI" old-poison; verify_poi "$S/build/old-poison" old-poison ;;&
  *) ;;
esac
