#!/bin/sh

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
#
# mace4_cells_test.sh -- Mace4 refuses an impossible cell table instead of
#                        crashing on it or exhausting the machine.
#
# ---------------------------------------------------------------------------
# WHAT WAS WRONG
#
# Mace4 searches for a finite model by tabulating every tuple of every
# function and relation: a symbol of arity a over a domain of size d needs
# d^a cells, and init_for_domain_size() allocates them all up front.  Three
# things about that allocation were unguarded:
#
#   1. The total was accumulated in an int.  int_power() saturates at
#      INT_MAX, and the caller then did "nextbase += INT_MAX", so any symbol
#      of arity >= 31 at domain size 2 wrapped silently and the table was
#      allocated at a size unrelated to what the code went on to index.
#
#   2. The malloc return was never checked.  The next statement was
#      "c->id = id", so a failed allocation was a NULL dereference, i.e. an
#      immediate SIGSEGV, before any searching had been done at all.
#
#   3. The bytes were invisible to max_megs.  The tables come from malloc(),
#      not palloc(), so megs_malloced() cannot see them, and mace4 even calls
#      disable_max_megs() because "mace4 does its own max_megs check" -- a
#      check which then could not account for its single largest allocation.
#
# High arities are not an exotic input: they arrive from clausification.
# Skolemising a formula with n nested quantifiers produces a function of
# arity n.  TPTP Problems/LCL/LCL651+1.010.p -- a modal logic problem whose
# own maximum arity is 2 -- clausifies to Skolem functions f3 through f29,
# whose tables come to 2^30 cells.  At domain size 2 that is an 80 GB
# request, made before the first inference.
#
# MEASURED CONSEQUENCES, both on this machine:
#
#   * With a memory limit in force, malloc returned NULL and Mace4 died in
#     SIGSEGV.  In a 300-second TPTP sweep, 38 of the first 618 runs (6.1%)
#     ended that way, none having searched anything.
#
#   * With no memory limit, Linux granted the 80 GB reservation and Mace4
#     touched enough of it to drive the machine into swap.  One such run was
#     measured at 85.5 GB resident.  A sweep of them exhausted all 7 GB of
#     swap and left the machine unable to fork a login shell, which cost a
#     reboot.
#
# ---------------------------------------------------------------------------
# WHAT THE FIX DOES
#
# init_for_domain_size() now returns BOOL.  Before allocating anything it
# totals the table in 64-bit arithmetic (CellCountType), and refuses when
# either
#
#   (a) the cell count would exceed MAX_CELLS, so the ids could not be held
#       in the int that struct cell and Number_of_cells use, or
#   (b) the bytes would exceed what is left of max_megs.
#
# Every allocation is then checked, Cells is calloc'd so that a failure part
# way through leaves each unreached possible[] NULL and safe to free, and
# mace_megs() adds the table in so the ongoing check stays honest.  A refusal
# reports max_megs and stops the search, because a larger domain cannot need
# fewer cells.
#
# ---------------------------------------------------------------------------
# WHAT THIS TEST ASSERTS
#
# Two inputs, each a single clause with one wide predicate, which is all it
# takes.  Both must exit 7 (max_megs_no) with the matching diagnostic, and
# must do so without allocating the table.  Against the pristine build both
# exit 102 (SIGSEGV) under a memory cap, which is what makes this a
# regression test rather than a description.
#
# Run with BIN=<dir> to test a different build; default is ../bin.
# ---------------------------------------------------------------------------

BIN=${BIN:-../bin}
MACE4=$BIN/mace4
fail=0
tmp=${TMPDIR:-/tmp}/mace4_cells_test.$$
mkdir -p "$tmp" || exit 2
trap 'rm -rf "$tmp"' 0 1 2 3 15

# args N -> "x1,x2,...,xN"
args() {
  i=2
  printf 'x1'
  while [ $i -le "$1" ]; do printf ',x%d' $i; i=$((i + 1)); done
}

mkinput() {       # mkinput FILE SYMBOL ARITY
  {
    echo 'assign(max_megs,500).'
    echo 'formulas(sos).'
    printf '%s(%s).\n' "$2" "$(args "$3")"
    echo 'end_of_list.'
  } > "$1"
}

check() {         # check LABEL FILE EXPECTED_PATTERN
  label=$1; file=$2; pattern=$3
  out=$tmp/out
  "$MACE4" -f "$file" > "$out" 2>&1
  rc=$?
  if [ "$rc" -ne 7 ]; then
    echo "FAIL: $label: expected exit 7 (max_megs_no), got $rc"
    [ "$rc" -eq 102 ] && echo "      exit 102 is SIGSEGV -- the unguarded allocation is back"
    fail=$((fail + 1))
    return
  fi
  if ! grep -q "$pattern" "$out"; then
    echo "FAIL: $label: exit 7 but no diagnostic matching: $pattern"
    fail=$((fail + 1))
    return
  fi
  echo "ok:   $label"
}

# (a) arity 40: 2^40 cells cannot be indexed by an int at all.  Upstream
#     saturated int_power() at INT_MAX and then overflowed the accumulator.
mkinput "$tmp/wide.in" p 40
check "arity 40 refused: cell ids would not fit in an int" \
      "$tmp/wide.in" 'cell ids are int'

# (b) arity 29: the count does fit in an int, but the table is ~40 GB, far
#     beyond max_megs.  This is the shape LCL651+1.010 produces.
mkinput "$tmp/big.in" q 29
check "arity 29 refused: table exceeds max_megs" \
      "$tmp/big.in" 'exceeds max_megs'

# (c) The refusal must be cheap.  If the table were actually allocated, this
#     would take far longer than a second and show up as a huge RSS; the
#     point of checking before allocating is that it costs nothing.
start=$(date +%s)
"$MACE4" -f "$tmp/big.in" >/dev/null 2>&1
elapsed=$(( $(date +%s) - start ))
if [ "$elapsed" -le 5 ]; then
  echo "ok:   refusal is immediate (${elapsed}s), nothing was allocated"
else
  echo "FAIL: refusal took ${elapsed}s -- the table is being allocated first"
  fail=$((fail + 1))
fi

# (c2) The negative-propagation index, which is the bigger of the two.
#
#      init_recurse() in negpropindex.c builds a tree of depth "arity" whose
#      branching factor is Domain_size+1, so it allocates (d+1)^a pointers --
#      growing faster than the cell table's d^a -- and negprop is on by
#      default.  It was unaccounted and unchecked in the same way:
#      "p[i] = init_recurse(n-1)" writes through p without testing it.
#
#      This was found only after the cell-table guard was already merged: a
#      sweep relaunched with that fix still reached a 160 GB cgroup ceiling
#      in twenty seconds, and 21 of 50 runs still died in SIGSEGV.  Measured
#      with the guard in place, these are what the refused runs actually
#      wanted: LCL655+1.015 23.7 GB, ANA009-1 35.5 GB, SWV482+2 79.9 GB,
#      LCL659+1.020 559 GB, NLP229-10 1,436 GB.
#
#      An arity-15 relation at domain size 2 is (3^15-1)/2 = 7,174,453 nodes
#      of 3 pointers each, twice over: 344 MB, against a cell table for the
#      same symbol of 32,768 cells, about 2.4 MB.  That ratio of 140x is the
#      point -- with negprop uncounted the cell-table check passes easily and
#      the run still exhausts memory.
{
  echo 'assign(max_megs,20).'
  echo 'formulas(sos).'
  printf 'r(%s).\n' "$(args 15)"
  echo 'end_of_list.'
} > "$tmp/np.in"
out=$tmp/npout
"$MACE4" -f "$tmp/np.in" > "$out" 2>&1
rc=$?
if [ "$rc" -eq 7 ] && grep -q 'negprop index' "$out"; then
  echo "ok:   negprop index is counted and names itself in the refusal"
elif [ "$rc" -eq 102 ]; then
  echo "FAIL: exit 102 is SIGSEGV -- the negprop index is unguarded again"
  fail=$((fail + 1))
else
  echo "FAIL: negprop index not accounted (exit $rc, expected 7 naming negprop)"
  fail=$((fail + 1))
fi

#      And clearing negprop must actually leave a searchable problem, or the
#      advice the refusal prints would be wrong.
{
  echo 'assign(max_megs,20).'
  echo 'assign(max_seconds,5).'
  echo 'clear(negprop).'
  echo 'formulas(sos).'
  printf 'r(%s).\n' "$(args 15)"
  echo 'end_of_list.'
} > "$tmp/np2.in"
"$MACE4" -f "$tmp/np2.in" > "$tmp/np2out" 2>&1
rc=$?
if [ "$rc" -ne 7 ] && [ "$rc" -ne 102 ]; then
  echo "ok:   clear(negprop) leaves the same problem searchable (exit $rc)"
else
  echo "FAIL: clear(negprop) did not help (exit $rc); the refusal's advice is wrong"
  fail=$((fail + 1))
fi

# (c3) max_megs must bound GROUND-CLAUSE GENERATION, which happens before the
#      first search node.
#
#      Mace4 calls disable_max_megs() because it "does its own max_megs
#      check" -- and that check is in check_time_memory(), called from the top
#      of search().  Everything mace4n() does before search() was therefore
#      unbounded: generate_ground_clauses() instantiates every clause over the
#      domain, three passes, and at a large domain size that is where the
#      memory goes.  With LADR's check off and Mace4's not yet reachable, the
#      only ceiling was the operating system's.
#
#      Measured on the 300 s TPTP sweep: of 59 FATAL runs, 38 died as "palloc,
#      operating system is out of memory" rather than at their stated 2000 MB
#      max_megs, every one during "DOMAIN SIZE n" before searching.  Uncapped
#      and run many-wide, that is what exhausts a machine's swap.
#
#      The input below is two clauses with eight distinct variables, forced to
#      domain size 10 with -n/-N: 10^8 ground instances per clause, which no
#      20 MB ceiling can hold.  Against pristine and against the build with
#      only the cell-table and negprop fixes, this exits 1 with "palloc,
#      operating system is out of memory"; it must exit 7 and say what it was
#      doing.
{
  echo 'assign(max_megs,20).'
  echo 'assign(max_seconds,60).'
  echo 'formulas(sos).'
  echo 'p(x1,x2) | p(x3,x4) | p(x5,x6) | p(x7,x8) | q(x1,x3,x5,x7).'
  echo '-p(x1,x2) | -q(x1,x2,x3,x4) | r(x3,x4).'
  echo 'end_of_list.'
} > "$tmp/ground.in"
out=$tmp/groundout
"$MACE4" -n 10 -N 10 -f "$tmp/ground.in" > "$out" 2>&1
rc=$?
if [ "$rc" -eq 7 ] && grep -q 'before the search could proceed' "$out"; then
  echo "ok:   max_megs bounds ground-clause generation, before the first search node"
elif [ "$rc" -eq 1 ] && grep -q 'operating system is out of memory' "$out"; then
  echo "FAIL: ground-clause generation ran to the OS limit -- max_megs is not"
  echo "      enforced before search() again (disable_max_megs is back?)"
  fail=$((fail + 1))
else
  echo "FAIL: expected exit 7 naming the pre-search phase, got $rc"
  fail=$((fail + 1))
fi

# (d) A table that does fit must still be searched exactly as before, or the
#     guard has broken ordinary model finding.  Two unary predicates over a
#     domain of 2 is 4 cells; Mace4 must find a model and exit 0.
{
  echo 'formulas(sos).'
  echo 'a(x) | b(x).'
  echo 'end_of_list.'
} > "$tmp/ok.in"
"$MACE4" -f "$tmp/ok.in" > "$tmp/ok.out" 2>&1
rc=$?
if [ "$rc" -eq 0 ] && grep -q 'Exiting with 1 model' "$tmp/ok.out"; then
  echo "ok:   a table that fits is still searched, and the model is found"
else
  echo "FAIL: a feasible problem no longer yields a model (exit $rc)"
  fail=$((fail + 1))
fi

if [ "$fail" -eq 0 ]; then
  echo "ALL PASSED (0 failure(s))"
  exit 0
else
  echo "$fail failure(s)"
  exit 1
fi
