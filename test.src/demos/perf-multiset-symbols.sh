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
# A measurement, not a pass/fail test: how long Prover9 takes just to READ an
# input file containing n distinct symbols.  No search is performed -- the
# clauses are satisfiable units and max_given=0 stops the loop after the first
# given clause -- so what is timed is startup: parsing, symbol gathering,
# precedence assignment and indexing.
#
# THE DEFECT
#   Two quadratics, on the same data, in the same phase of startup.
#
#   1. BUILDING the symbol multiset.  LADR represents "the multiset of symbols
#      in the input" as an I2list of (symbol, count) pairs, and
#      multiset_add_n() searched that list linearly for every symbol it added:
#
#          if (a == NULL)        { a = get_i2list(); ... }
#          else if (a->i == i)     a->j += n;
#          else                    a->next = multiset_add_n(a->next, i, n);
#
#      Building a multiset of n distinct symbols therefore cost O(n^2)
#      comparisons.  It was also recursive, one stack frame per distinct
#      symbol, so at 250,000 symbols it overflowed the 8 MB default stack and
#      the process died with no diagnostic at all.  multiset_to_set() recursed
#      over the same list for the same reason.
#
#   2. READING it back.  lex_order() in symbols.c assigns the symbol
#      precedence that the term ordering is derived from, and did this:
#
#          for (p = fsyms; p; p = p->next, i++)
#            a[i]->occurrences = multiset_occurrences(fsyms_multiset, p->i);
#
#      -- and multiset_occurrences() walks the multiset looking for one
#      symbol.  Once per symbol over all symbols is O(n^2) again, so fixing
#      only the building half would have left startup quadratic.
#
# THE FIX
#   glist.c gains I2accum, the same multiset abstraction backed by an
#   open-addressed hash table: O(1) amortised insertion and O(1) lookup.  The
#   gather routines recurse over an I2accum; the public entry points keep
#   their "I2list *" signatures and convert once in and once out, so no caller
#   changed.  "Once" is the design -- a wrapper inside the per-formula loop
#   would convert n times and put the quadratic straight back, which is why
#   gather_symbols_in_formulas() wraps the loop and not the formula.
#
#   First-seen ordering is preserved exactly.  That is not a nicety: symbol
#   precedence comes from these lists, precedence fixes the term ordering, and
#   the term ordering decides which equations become rewrite rules.  A
#   different order here would silently change every proof.
#   ../multiset_test.c asserts the two implementations agree element for
#   element AND in order.
#
# WHAT THIS MEASURES, AND WHAT IT DOES NOT  -- read this before quoting a number
#
#   Profiling the fixed build at 6,000 symbols put i2accum_add_n at 0.12% of
#   the run.  The multiset is simply no longer on the profile.  But Prover9's
#   startup at that scale is still dominated by other superlinear work, and
#   honesty about which is which matters more than a large ratio:
#
#   (a) predicate_elimination, in provers.src/pred_elim.c, is O(R^3) in the
#       number of relation symbols and is ON by default (auto_setup implies
#       predicate_elim).  Its driver reads:
#
#           syms = eliminable_relations(clauses2, equality);
#           while (syms) {
#             clauses2 = elim_relation(syms->i, clauses2, disabled, echo);
#             zap_ilist(syms);                     /* discards the rest */
#             syms = eliminable_relations(clauses2, equality);
#           }
#
#       -- "use first symbol, discard rest, get new list", as its own comment
#       says.  One symbol is eliminated per round and the entire candidate set
#       is recomputed, and each recomputation calls eliminable_relation() once
#       per relation symbol, each call scanning every clause.  At 3,000
#       predicates the profile is 37.6% eliminable_relations, 22.4%
#       plist_append (9,000,012 calls) and 11.3% ilist_append.  This dwarfs
#       everything else and is a separate fix.
#
#   (b) With predicate_elim cleared, what remains is still quadratic:
#       lookup_by_id at 384 million calls and roughly 768 million Ilist
#       get/prepend/free pairs, in the feature-vector and FPA indexing path.
#       Also a separate fix.
#
#   So this script prints BOTH configurations.  The right column to read for
#   the multiset change is the "predicate_elim cleared" one, because with
#   predicate_elim on, (a) is what is being timed.
#
# MEASURED
#
#   Library level, driving the multiset functions directly -- this is the
#   clean statement of the change:
#
#       multiset_to_set, 500,000 elements    before: SIGSEGV   after: returns
#       multiset_add, 64,000 distinct        before: 7.59 s    after: 3.33 s
#       I2accum, 200,000 distinct inserts    before: n/a       after: 0.008 s
#       I2accum, 200,000 lookups             before: n/a       after: 0.003 s
#
#   The 64,000 row is the small-multiset interface, still a linear search by
#   design and merely no longer recursive.  The hash accumulator is what the
#   unbounded paths use.
#
#   End to end, Prover9 startup, predicate_elim cleared:
#
#       symbols      before      after
#        2,000        0.27 s     0.23 s
#        4,000        1.27 s     1.06 s
#        8,000        5.81 s     4.67 s
#       16,000       25.90 s    19.48 s      1.3x
#
#   End to end with the default predicate_elim, for contrast -- both columns
#   are dominated by (a), which is why the ratio means little here:
#
#        4,000       68.58 s    30.44 s
#
# USAGE
#   perf-multiset-symbols.sh [BASELINE_BIN_DIR]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
new="$here/../../bin/prover9"
old="${1:-}"

[ -x "$new" ] || { echo "no prover9 at $new -- run make first" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# n distinct unary predicates, each applied to its own distinct constant: 2n
# distinct symbols, n clauses, nothing to prove.
generate() {
  local n="$1" out="$2" elim="$3" i
  {
    echo 'set(quiet).'
    echo 'clear(print_initial_clauses).'
    [ "$elim" = off ] && echo 'clear(predicate_elim).'
    echo 'assign(max_given, 0).'
    echo 'formulas(sos).'
    for ((i = 0; i < n; i++)); do
      echo "p$i(c$i)."
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
    137|152) echo "KILLED" ;;
    *) printf '%.2f s' "$(echo "$t1 - $t0" | bc)" ;;
  esac
}

ladder() {
  local elim="$1"; shift
  echo
  echo "### predicate_elim $elim"
  printf '%-12s %-14s %-14s %s\n' 'SYMBOLS' 'BEFORE' 'AFTER' 'RATIO'
  printf '%s\n' '------------------------------------------------------------'
  for n in "$@"; do
    generate "$n" "$tmp/in.$n.$elim" "$elim"
    a="n/a"
    [ -n "$old" ] && [ -x "$old/prover9" ] && a=$(run "$old/prover9" "$tmp/in.$n.$elim")
    b=$(run "$new" "$tmp/in.$n.$elim")
    r=""
    [ "$a" != "n/a" ] && [ "${a: -1}" = "s" ] && [ "${b: -1}" = "s" ] &&
      r=$(printf '%.1fx' "$(echo "${a% s} / ${b% s}" | bc -l)")
    printf '%-12s %-14s %-14s %s\n' "$((n * 2))" "$a" "$b" "$r"
  done
}

# The configuration that isolates this change.
ladder off 1000 2000 4000 8000

# The default configuration, where predicate_elimination dominates.  Small
# sizes only: at 4,000 predicates the unfixed build needs over a minute, and
# the growth is cubic.
ladder on 500 1000 2000

printf '%s\n' '------------------------------------------------------------'
cat <<'NOTE'
Counting distinct symbols as 2n: n predicates and n constants.

Read the "predicate_elim off" table for this change: the old column should
roughly quadruple for each doubling and the new column should too, because
what remains there is the indexing quadratic noted in the header -- the
multiset is off the profile entirely (0.12% at 6,000 symbols) and the 1.1x to
1.3x is what removing it is worth once it is no longer the bottleneck.

The "predicate_elim on" table is timing predicate_elimination's O(R^3) loop,
not this change.  It is here so that nobody mistakes one for the other.
NOTE
