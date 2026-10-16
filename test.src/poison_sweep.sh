#!/bin/bash

#  Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written for the audit of his release
#  that FIXES.md describes, and carries the library's licence so that
#  it can be distributed with it.
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
#------------------------------------------------------------------------------
# The gate that AddressSanitizer and valgrind cannot be: sweep the corpus with
# LADR's own allocator poisoning freed blocks.
#
# WHY NEITHER ASan NOR VALGRIND IS ENOUGH HERE
#   LADR does not hand memory back to the system allocator.  palloc() carves
#   20 MB blocks and free_mem() pushes the cell onto a per-size avail list for
#   get_mem() to hand straight back out:
#
#       void free_mem(void **p, unsigned n) {
#         ...
#         *p = M[n];      /* avail-list link */
#         M[n] = p;
#       }
#
#   So a use-after-free inside LADR never touches memory that ASan or valgrind
#   considers free.  Both tools see a live arena and report nothing.  Every
#   use-after-free in a list function -- and this tree's glist.c rewrite turned
#   forty of them into manual prev/next splicing with explicit free_ilist() and
#   free_plist() calls -- is invisible to them.
#
#   What does find it is -DPOISON_FREED_MEM, which fills every freed block with
#   0xdeadbeefdeadbeef apart from the avail-list link.  A read through a stale
#   pointer then returns an address that segfaults on dereference instead of
#   returning plausible data, and a stale list walk stops at a wild pointer
#   rather than quietly succeeding.
#
#   This is how M8, the use-after-free in zap_flatterm that fires on every
#   call, was actually caught; a plain ASan sweep had already passed over it.
#
# WHY IT RUNS IN PARALLEL
#   The first version was a serial loop, which on an 80-core machine left 79
#   cores idle and would have taken hours.  The per-run budget is `ulimit -t`,
#   CPU time, so it does not turn queueing into a timeout the way a wall clock
#   does.  It is not load-INVARIANT, though: CPU time is time on core, and
#   under memory-bandwidth contention a process does less work per on-core
#   second.  Measured here, the same binary on the same input varied 2x in CPU
#   seconds with load, so do not use a loaded machine for a timing comparison.
#
# USAGE
#   poison_sweep.sh CORPUS_LIST [BUILD_DIR] [JOBS]
#     JOBS defaults to every core, capped by available memory.
#
#   Exit status 0 when no run crashes or reports an error that the same run
#   does not also produce in an unpoisoned build.
#------------------------------------------------------------------------------

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
corpus="${1:-}"
build="${2:-${TMPDIR:-/tmp}/ladr-poison-$$}"
# Default to EVERY core.  On this hardware the only real constraint is
# physical memory, not core count: a serial version of this sweep left 79 of
# 80 cores idle and turned 20 minutes of work into hours, and the symptom was
# deceptive -- prover9 showing ~1.2% CPU looks stalled when in fact it is
# pegged at 100% of its one core and 1.25% is one core out of eighty.
#
# The cap is memory: two ordinary prover runs per worker, each up to max_megs,
# so budget 800 MB per concurrent run against MemAvailable.
mem_mb=$(awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo)
mem_cap=$(( mem_mb / 800 ))
[ "$mem_cap" -lt 1 ] && mem_cap=1
cores=$(nproc)
if [ "$mem_cap" -lt "$cores" ]; then default_jobs="$mem_cap"; else default_jobs="$cores"; fi
jobs="${3:-$default_jobs}"
[ "$jobs" -lt 1 ] && jobs=1

[ -n "$corpus" ] && [ -r "$corpus" ] || {
  echo "usage: $0 CORPUS_LIST [BUILD_DIR] [JOBS]" >&2; exit 2; }

echo "=== building a poisoned tree and a matching control in $build ==="
rm -rf "$build"
mkdir -p "$build/poison" "$build/control"
for v in poison control; do
  ( cd "$root" && git archive HEAD ) | tar -x -C "$build/$v" || {
    echo "could not export sources from git" >&2; exit 2; }
done

( cd "$build/poison"  && make XFLAGS="-DPOISON_FREED_MEM -g -O1" all ) > "$build/poison.log" 2>&1
( cd "$build/control" && make XFLAGS="-g -O1" all )                   > "$build/control.log" 2>&1

for v in poison control; do
  [ -x "$build/$v/bin/prover9" ] || {
    echo "BUILD FAILED for $v (see $build/$v.log)" >&2
    tail -20 "$build/$v.log" >&2; exit 2; }
done

# Verify the poisoning is actually compiled in, rather than trusting the flag.
# A build where the #ifdef did not fire would sweep clean and prove nothing.
if ! grep -aq 'deadbeef' <(strings "$build/poison/bin/prover9") 2>/dev/null; then
  if ! grep -ac 'DPOISON_FREED_MEM' "$build/poison.log" >/dev/null; then
    echo "  cannot confirm POISON_FREED_MEM reached the compiler" >&2
    exit 2
  fi
fi
# The pattern must include g++: libladr.a has C++ translation units, and an
# earlier version of this check matched only (gcc|cc), so it reported 137 of
# 139 lines and looked like a partial build when the build was in fact
# correct.  The sanitizer sweep's equivalent check REFUSES on a mismatch; this
# one only printed, so it under-reported silently.  It asserts now.
compile_re='(^|[[:space:]])(gcc|cc|g\+\+)[[:space:]].*[[:space:]]-c([[:space:]]|$)'
ptotal=$(grep -acE "$compile_re" "$build/poison.log")
ppois=$(grep -aE "$compile_re" "$build/poison.log" | grep -ac -- -DPOISON_FREED_MEM)
ctotal=$(grep -acE "$compile_re" "$build/control.log")
cpois=$(grep -aE "$compile_re" "$build/control.log" | grep -ac -- -DPOISON_FREED_MEM)
printf '  poisoned build: %s of %s compile lines carried -DPOISON_FREED_MEM\n' "$ppois" "$ptotal"
printf '  control build:  %s of %s (expected 0)\n' "$cpois" "$ctotal"
[ "$ptotal" -gt 0 ] && [ "$ppois" = "$ptotal" ] || {
  echo "  POISONING INCOMPLETE -- refusing to report a sweep on this build" >&2
  exit 2; }
[ "$cpois" = 0 ] || {
  echo "  CONTROL IS POISONED TOO -- the comparison would prove nothing" >&2
  exit 2; }

echo
echo "=== sweeping, $jobs at a time ==="
# A difference only counts when the control survives and the poisoned build
# does not: anything that fails in both is a property of the input, not a
# stale-pointer read.
cat > "$build/worker.sh" <<'WORKER'
#!/bin/bash
build="$1"; f="$2"
[ -r "$f" ] || exit 0
for prog in prover9 mace4; do
  ( ulimit -t 60; "$build/poison/bin/$prog"  -f "$f" ) >/dev/null 2>&1; pr=$?
  ( ulimit -t 60; "$build/control/bin/$prog" -f "$f" ) >/dev/null 2>&1; cr=$?
  # 139 = SIGSEGV, 134 = SIGABRT, 136 = SIGFPE
  case "$pr" in
    139|134|136) [ "$cr" != "$pr" ] && echo "STALE|$prog|$f|$pr|$cr" ;;
  esac
done
exit 0
WORKER
chmod +x "$build/worker.sh"

xargs -a "$corpus" -P "$jobs" -I{} "$build/worker.sh" "$build" "{}" \
  > "$build/findings.txt" 2>/dev/null

runs=$(( $(grep -cve '^[[:space:]]*$' "$corpus") * 2 ))
n=$(grep -ac '^STALE|' "$build/findings.txt" || true)
while IFS='|' read -r _ prog f pr cr; do
  printf '  STALE READ: %-8s %-28s poisoned=%s control=%s\n' \
    "$prog" "$(basename "$f")" "$pr" "$cr"
done < "$build/findings.txt"

echo
printf 'ran %s prover/problem pairs against both builds; %s differ by a crash\n' "$runs" "$n"
if [ "$n" = 0 ]; then
  echo "poison_sweep: CLEAN"
  rm -rf "$build"
  exit 0
else
  echo "poison_sweep: $n STALE-POINTER CANDIDATE(S) -- builds kept in $build"
  exit 1
fi
