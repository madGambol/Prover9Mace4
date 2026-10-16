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
# A measurement, not a pass/fail test: how long Prover9 takes to start up on a
# file containing n equational definitions of the same symbol.  max_given=0
# stops the loop after the first given clause, so this is startup alone.
#
# THE DEFECT
#   eq_defs() in provers.src/unfold.c finds the equational definitions in the
#   input -- clauses like f(x) = h(x,a), which the prover can then unfold --
#   and records, per defined symbol, which function symbols its right-hand
#   side depends on.  It is on by default: auto_setup implies eq_defs=unfold.
#
#   It accumulated those dependents like this:
#
#       table[symbol].dependents = ilist_cat(table[symbol].dependents,
#                                            fsym_set_in_term(beta));
#
#   and ilist_cat() finds the end of its first argument by walking it.  So
#   every definition of a symbol walked everything already recorded for that
#   symbol: quadratic in the number of definitions of one symbol.
#
#   Two smaller costs in the same loop had the same shape --
#   ilist_member(symbols, symbol) scanned the symbols already seen, once per
#   definition, and ilist_append(symbols, symbol) then walked that same list
#   again to reach its tail.
#
# WHY IT MATTERS, MEASURED ON A REAL PROBLEM
#   Not hypothetically.  On Problems/CSR/CSR040-10.p (44,227 clauses after
#   translation), instrumenting the loop gives:
#
#       definitions found                      22,510
#       DISTINCT symbols being defined              1
#       length of that symbol's dependent list 95,234
#
#   One symbol, 22,510 definitions, about 4.8 dependents each.  ilist_cat
#   therefore walked an average of ~48,000 cells 22,510 times: about 1.07
#   billion cell visits, which gprof measured as 15.57 s and 51% of the entire
#   run once the feature-vector work in PF4 was done.
#
#   And every bit of it was wasted: all 22,510 of those definitions turn out to
#   be involved in cycles, so eliminate_cycles() discards them and the run
#   reports "Unfolding symbols: (none)."  Before and after this change, equally
#   -- the point is that it reaches that answer 2x faster.
#
# THE FIX
#   A tail pointer per symbol (struct symdata gains dependents_tail), so
#   appending is O(1); a `listed` flag per symbol instead of
#   ilist_member(symbols, ...); and a tail pointer for the symbols list
#   instead of ilist_append.  Nothing about the result changes: dependents
#   keeps the same elements in the same order, duplicates across definitions
#   included -- the original concatenated sets without merging them, so those
#   duplicates were already there -- and symbols keeps each symbol once, in
#   first-seen order.
#
#   eliminate_cycles() and insert_symbol() were also recursive once per
#   symbol, which is the PF5 stack-depth hazard; both are loops now, with the
#   same comparison sequence and the same insertion points.
#
# MEASURED
#
#   Startup on CSR040-10, three runs each:
#
#       before   32.42  32.45  31.40 s
#       after    15.76  16.13  15.71 s        2.05x
#
#   And cumulatively, with PF2, PF3, PF4 and PF5:
#
#       pristine upstream   168.17 s
#       this tree            15.54 s          10.8x
#
#   The ladder this script prints uses n definitions of ONE symbol drawn from
#   a fixed vocabulary of about 42 symbols.  That last part matters: an
#   earlier version of this demo introduced two new constants per definition,
#   and was then dominated by a DIFFERENT quadratic -- collect_multiples() in
#   ladr/symbols.c compares every symbol name against every other -- which
#   masked the effect entirely and showed 1.0x at every size.  Representative
#   figures with the vocabulary held fixed:
#
#       definitions    before     after    ratio
#         2,000         0.08 s    0.08 s    1.0x
#         4,000         0.25 s    0.17 s    1.4x
#         8,000         0.88 s    0.51 s    1.7x
#        16,000         4.83 s    1.65 s    2.9x
#
#   The ratio GROWING with n is the result being demonstrated; any single
#   figure in that table is just one point on it.
#
# USAGE
#   perf-eq-defs.sh [BASELINE_BIN_DIR]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
new="$here/../../bin/prover9"
old="${1:-}"

[ -x "$new" ] || { echo "no prover9 at $new -- run make first" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# n distinct definitions of the single symbol f, over a vocabulary of about 42
# symbols: f, h, x and a0..a39.  Holding the vocabulary fixed is what keeps
# this measuring eq_defs rather than the all-pairs symbol-name check.
generate() {
  local n="$1" out="$2" i=0 a b c
  {
    echo 'set(quiet).'
    echo 'clear(print_initial_clauses).'
    echo 'clear(predicate_elim).'
    echo 'assign(max_given, 0).'
    echo 'assign(max_megs, 8000).'
    echo 'formulas(sos).'
    for a in $(seq 0 39); do
      for b in $(seq 0 39); do
	for c in $(seq 0 39); do
	  [ "$i" -ge "$n" ] && break 3
	  echo "f(x) = h(x, a$a, a$b, a$c)."
	  i=$((i + 1))
	done
      done
    done
    echo 'end_of_list.'
  } > "$out"
}

run() {
  local bin="$1" f="$2" t0 t1 rc
  t0=$(date +%s.%N)
  ( ulimit -t 300; "$bin" -f "$f" ) >/dev/null 2>&1
  rc=$?
  t1=$(date +%s.%N)
  case "$rc" in
    139) echo "SEGV" ;;
    137|152) echo ">300 s (CPU)" ;;
    *) printf '%.2f s' "$(echo "$t1 - $t0" | bc)" ;;
  esac
}

printf '\n%-14s %-14s %-14s %s\n' 'DEFINITIONS' 'BEFORE' 'AFTER' 'RATIO'
printf '%s\n' '------------------------------------------------------------'
for n in 2000 4000 8000 16000; do
  generate "$n" "$tmp/in.$n"
  a="n/a"
  [ -n "$old" ] && [ -x "$old/prover9" ] && a=$(run "$old/prover9" "$tmp/in.$n")
  b=$(run "$new" "$tmp/in.$n")
  r=""
  [ "${a: -1}" = "s" ] && [ "${b: -1}" = "s" ] && [ "${a:0:1}" != ">" ] &&
    r=$(printf '%.1fx' "$(echo "${a% s} / ${b% s}" | bc -l)")
  printf '%-14s %-14s %-14s %s\n' "$n" "$a" "$b" "$r"
done
printf '%s\n' '------------------------------------------------------------'
cat <<'NOTE'
All n definitions are of the SAME symbol, over a fixed vocabulary of about 42
symbols, which is the shape found in Problems/CSR/CSR040-10.p: 22,510
definitions, all of one symbol, whose dependent list reached 95,234 entries.

The ratio should GROW with n -- that is the quadratic being removed.  Holding
the vocabulary fixed is deliberate: adding new symbols per definition brings in
collect_multiples(), which compares every symbol name against every other, and
that swamps this measurement.
NOTE
