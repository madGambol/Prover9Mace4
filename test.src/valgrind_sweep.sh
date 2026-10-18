#!/bin/bash

#  Copyright (C) 2026 Chris Arena, 2026-10-16.
#  Written in collaboration with Claude Opus 5 (Anthropic).
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
# Sweep a corpus under valgrind memcheck, with origin tracking.
#
# WHAT THIS CATCHES THAT THE OTHER TWO DO NOT
#   sanitizer_sweep.sh finds out-of-bounds access and undefined behaviour at
#   the points the compiler instrumented.  poison_sweep.sh finds stale reads
#   through LADR's own avail lists, which neither ASan nor valgrind can see.
#   valgrind adds reads of memory that was never written -- uninitialised
#   values -- which is the one class the other two miss, and it reports where
#   the value came from rather than only where it was used.
#
#   That mattered here once already, in the opposite direction: a run that
#   looked nondeterministic was investigated as uninitialised memory, and
#   valgrind with --track-origins reported 0 uninitialised-value reports and
#   "0 errors from 0 contexts", which is what ruled the whole class out and
#   turned the investigation towards the comparison harness, where the bug
#   actually was.  See "Determinism" in ../FIXES.md.
#
# WHY IT IS A SAMPLE
#   valgrind is 20-50x slower than native, so a full corpus sweep is hours.
#   This takes a sample size and walks the corpus at a stride, so the sample is
#   spread across the whole list rather than being its first n entries -- the
#   corpus is grouped by directory, and the first n would all be one family of
#   problems.
#
# USAGE
#   valgrind_sweep.sh CORPUS_LIST [SAMPLE_SIZE] [BUILD_DIR] [JOBS]
#     SAMPLE_SIZE defaults to 40 problems (so 80 prover/problem pairs).
#     JOBS defaults to every core, capped by available memory.  Parallelism is safe
#     because the per-run budget is `ulimit -t`, CPU time, which means the
#     same thing with sixty runs in flight as with one.
#
#   Exit status 0 when no run reports an error.
#------------------------------------------------------------------------------

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
corpus="${1:-}"
sample="${2:-40}"
build="${3:-${TMPDIR:-/tmp}/ladr-valgrind-$$}"
# Default to EVERY core.  On this hardware the only real constraint is
# physical memory, not core count: a serial version of this sweep left 79 of
# 80 cores idle and turned 20 minutes of work into hours, and the symptom was
# deceptive -- prover9 showing ~1.2% CPU looks stalled when in fact it is
# pegged at 100% of its one core and 1.25% is one core out of eighty.
#
# The cap is memory: memcheck shadows every byte it tracks,
# so budget 1500 MB per concurrent run against MemAvailable.
mem_mb=$(awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo)
mem_cap=$(( mem_mb / 1500 ))
[ "$mem_cap" -lt 1 ] && mem_cap=1
cores=$(nproc)
if [ "$mem_cap" -lt "$cores" ]; then default_jobs="$mem_cap"; else default_jobs="$cores"; fi
jobs="${4:-${JOBS:-}}"
case "$jobs" in
  '')        echo "$0: how many concurrent runs?  pass JOBS=n, or argument 4." >&2
             echo "  This machine allows at most $default_jobs: $cores core(s)," >&2
             echo "  and memory for $mem_cap run(s) at 800 MB each." >&2
             exit 2 ;;
  *[!0-9]*)  echo "$0: JOBS must be a whole number, got '$jobs'" >&2; exit 2 ;;
esac
[ "$jobs" -lt 1 ] && jobs=1
if [ "$jobs" -gt "$default_jobs" ]; then
  echo "    JOBS=$jobs is more than this machine can take; using $default_jobs"
  echo "    ($cores core(s), memory for $mem_cap run(s) at 800 MB each)"
  jobs="$default_jobs"
fi

[ -n "$corpus" ] && [ -r "$corpus" ] || {
  echo "usage: $0 CORPUS_LIST [SAMPLE_SIZE] [BUILD_DIR] [JOBS]" >&2; exit 2; }

# A path in the list may be relative, and is then resolved against the LIST
# FILE's own directory rather than the caller's cwd, so that a corpus can be
# cloned anywhere and its lists still work.  Absolute paths are left alone;
# blank lines and # comments are dropped.
_corpus_dir=$(cd "$(dirname "$corpus")" && pwd)
_corpus_resolved=$(mktemp "${TMPDIR:-/tmp}/ladr-corpus.XXXXXX")
trap 'rm -f "$_corpus_resolved"' 0 1 2 3 15
awk -v d="$_corpus_dir" '
  /^[[:space:]]*(#|$)/ { next }
  { if ($0 ~ /^\//) print $0; else print d "/" $0 }' "$corpus" > "$_corpus_resolved"
corpus="$_corpus_resolved"
command -v valgrind >/dev/null || { echo "valgrind not installed" >&2; exit 2; }

echo "=== building an unoptimised tree in $build ==="
rm -rf "$build"
mkdir -p "$build"
( cd "$root" && git archive HEAD ) | tar -x -C "$build" || {
  echo "could not export sources from git" >&2; exit 2; }
# -O0: valgrind reports line numbers, and optimisation both moves code and can
# turn a read of an uninitialised value into something it cannot see.
( cd "$build" && make XFLAGS="-g -O0" all ) > "$build/build.log" 2>&1
[ -x "$build/bin/prover9" ] || {
  echo "BUILD FAILED (see $build/build.log)" >&2
  tail -20 "$build/build.log" >&2; exit 2; }

total=$(grep -cve '^[[:space:]]*$' "$corpus")
stride=$(( total / sample ))
[ "$stride" -lt 1 ] && stride=1
echo "  corpus has $total entries; taking every ${stride}th"

echo
echo "=== sweeping under memcheck, $jobs at a time ==="
out="$build/vg"
mkdir -p "$out"
awk -v s="$stride" 'NF && NR % s == 0' "$corpus" > "$build/sample.txt"

cat > "$build/worker.sh" <<'WORKER'
#!/bin/bash
build="$1"; f="$2"
[ -r "$f" ] || exit 0
for prog in prover9 mace4; do
  log="$build/vg/$(basename "$f").$prog.log"
  # --error-limit=no so a run reports everything rather than capping at 1000;
  # the CPU limit is generous because everything is 20-50x slower here.
  ( ulimit -t 300; valgrind --tool=memcheck --track-origins=yes \
      --error-exitcode=0 --error-limit=no --log-file="$log" \
      "$build/bin/$prog" -f "$f" ) >/dev/null 2>&1
  if grep -qaE 'Invalid read|Invalid write|uninitialised|Invalid free|Mismatched free' "$log"; then
    echo "REPORT|$prog|$f"
  else
    rm -f "$log"
  fi
done
exit 0
WORKER
chmod +x "$build/worker.sh"

xargs -a "$build/sample.txt" -P "$jobs" -I{} "$build/worker.sh" "$build" "{}" \
  > "$build/findings.txt" 2>/dev/null

runs=$(( $(grep -cve '^[[:space:]]*$' "$build/sample.txt") * 2 ))
n=$(grep -ac '^REPORT|' "$build/findings.txt" || true)
while IFS='|' read -r _ prog f; do
  echo "  MEMCHECK REPORT: $prog $(basename "$f")"
  grep -aE 'Invalid read|Invalid write|uninitialised|Invalid free' \
    "$out/$(basename "$f").$prog.log" 2>/dev/null | head -3 | sed 's/^/      /'
done < "$build/findings.txt"

echo
printf 'ran %s prover/problem pairs under memcheck; %s produced a report\n' "$runs" "$n"
if [ "$n" = 0 ]; then
  echo "valgrind_sweep: CLEAN"
  rm -rf "$build"
  exit 0
else
  echo "valgrind_sweep: $n REPORT(S) -- logs kept in $out"
  exit 1
fi
