#!/bin/sh
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
