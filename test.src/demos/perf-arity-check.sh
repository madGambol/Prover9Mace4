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
# file with n distinct symbols.  max_given=0 stops the loop after the first
# given clause, so this is startup alone.
#
# THE DEFECT
#   arity_check() runs over the whole input to report any symbol NAME used with
#   more than one arity -- p(a) and p(a,b) in the same file, which is almost
#   always a typo and which Prover9 rejects.  It did it by comparing every
#   symbol against every other:
#
#       for (p1 = syms; p1; p1 = p1->next) {
#         char *s1 = sn_to_str(p1->i);
#         for (p2 = p1->next; p2; p2 = p2->next) {
#           char *s2 = sn_to_str(p2->i);
#           if (str_ident(s1, s2)) { ... }
#         }
#       }
#
#   An explicit O(n^2) string comparison over the input's entire vocabulary, to
#   answer a question that is nearly always "nothing wrong".  On
#   Problems/CSR/CSR040-10.p it was 239,225,001 of the 263,958,220 str_ident
#   calls in the profile -- the largest single item left after PF7.
#
#   It was also big enough to hide other things.  The first version of the PF7
#   demo added two constants per definition, which made this the dominant cost
#   and showed 1.0x at every size for a change that is really 2.9x.  A
#   quadratic in the vocabulary does not just cost time; it masks whatever else
#   you are trying to measure.
#
# THE FIX
#   Group by name instead of comparing pairs: build an array of
#   (name, symnum, position), sort it by name, and read off the runs of two or
#   more.  O(n log n) string comparisons.
#
#   The output is identical, ORDER INCLUDED, which matters more than usual
#   here: the caller turns the list straight into a fatal_input_error that
#   names the symbols in list order, so a different order is a different error
#   message.  The order is not one anyone would guess, either -- the pairwise
#   loop discovered groups in order of each group's first member, listed each
#   group's members in input order, and prepended every symbol, reversing the
#   lot.  ../arity_check_test.sh pins the exact strings, because the corpus
#   cannot: this is a fatal error on malformed input, so no well-formed problem
#   reaches it.
#
# MEASURED
#
#   Startup on CSR040-10, three runs each:
#
#       before   15.46  15.90  16.48 s
#       after     9.26   8.94   9.00 s      1.75x
#
#   Cumulatively, with PF2, PF3, PF4, PF5 and PF7:
#
#       pristine upstream   168.17 s
#       this tree             9.07 s        18.5x
#
#   On a ladder of n distinct symbols, none of them duplicated, so the check
#   finds nothing and does all that work for no answer -- the ordinary case:
#
#       symbols      before      after    ratio
#        8,000        1.06 s     0.54 s    1.9x
#       16,000        4.85 s     2.46 s    1.9x
#       32,000       19.42 s     9.64 s    2.0x
#       64,000       81.11 s    29.46 s    2.7x
#
#   And on input that does trigger the error -- n names each used with two
#   arities, so there are n groups rather than one -- where the quadratic is
#   paid before the message can be printed at all:
#
#       groups       before      after    ratio
#        2,000        0.14 s     0.02 s     7.0x
#        4,000        0.59 s     0.06 s     9.8x
#        8,000        2.50 s     0.12 s    20.8x
#
#   The second table is the clearer demonstration: the before column
#   quadruples for each doubling, the after column roughly doubles.
#
#   Note that the first table's after column still quadruples.  Something else
#   there is still quadratic -- PF9, nonneg_clauses() appending to the list it
#   is growing, and the discrimination-index construction.  This fix removes
#   one quadratic from a startup path that has more than one.
#
# USAGE
#   perf-arity-check.sh [BASELINE_BIN_DIR]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
new="$here/../../bin/prover9"
old="${1:-}"

[ -x "$new" ] || { echo "no prover9 at $new -- run make first" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# n distinct symbols, no duplicated names: the check finds nothing, which is
# the ordinary case and the one every run pays for.
generate_clean() {
  local n="$1" out="$2" i
  {
    echo 'set(quiet).'
    echo 'clear(print_initial_clauses).'
    echo 'clear(predicate_elim).'
    echo 'assign(max_given, 0).'
    echo 'assign(max_megs, 8000).'
    echo 'formulas(sos).'
    for ((i = 0; i < n; i++)); do
      echo "p$i(c$i)."
    done
    echo 'end_of_list.'
  } > "$out"
}

# n names each used with two arities: n groups, and a fatal error at the end.
generate_bad() {
  local n="$1" out="$2" i
  {
    echo 'formulas(sos).'
    for ((i = 0; i < n; i++)); do
      echo "p$i(a). p$i(a,b)."
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

ladder() {
  local gen="$1" label="$2"; shift 2
  echo
  echo "### $label"
  printf '%-12s %-14s %-14s %s\n' 'SIZE' 'BEFORE' 'AFTER' 'RATIO'
  printf '%s\n' '------------------------------------------------------------'
  for n in "$@"; do
    "$gen" "$n" "$tmp/in.$n.$label"
    a="n/a"
    [ -n "$old" ] && [ -x "$old/prover9" ] && a=$(run "$old/prover9" "$tmp/in.$n.$label")
    b=$(run "$new" "$tmp/in.$n.$label")
    r=""
    [ "${a: -1}" = "s" ] && [ "${b: -1}" = "s" ] && [ "${a:0:1}" != ">" ] &&
      r=$(printf '%.1fx' "$(echo "${a% s} / ${b% s}" | bc -l)")
    printf '%-12s %-14s %-14s %s\n' "$n" "$a" "$b" "$r"
  done
}

ladder generate_clean "distinct symbols, nothing duplicated (the ordinary case)" \
  4000 8000 16000 32000
ladder generate_bad "names used with two arities, so n groups (the error case)" \
  1000 2000 4000 8000

printf '%s\n' '------------------------------------------------------------'
cat <<'NOTE'
The second table is the clearer demonstration: the before column should
quadruple for each doubling of the input, the after column roughly double.

The first table's after column still quadruples, because that startup path has
more than one quadratic in it -- see PF9 in ../../FIXES.md.  This fix removes
one of them.
NOTE
