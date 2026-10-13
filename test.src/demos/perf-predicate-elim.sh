#!/bin/bash
#------------------------------------------------------------------------------
# A measurement, not a pass/fail test: how long Prover9 takes to start up on a
# file with n distinct predicates, with its DEFAULT settings.  No search is
# performed -- max_given=0 stops the loop after the first given clause -- so
# this is startup alone.
#
# THE DEFECT
#   predicate_elimination() in provers.src/pred_elim.c was O(R^3) in the number
#   of relation symbols, and it is on by default: auto_setup implies
#   predicate_elim, and it runs whenever `usable` is empty, which is the
#   ordinary case for a file of axioms.  Three separate costs compounded.
#
#   1. The driver eliminates ONE symbol per round and then recomputes the whole
#      candidate set.  Its own comment says so:
#
#          Ilist syms = eliminable_relations(clauses2, equality);
#          while (syms) {
#            /* use first symbol, discard rest, get new list */
#            clauses2 = elim_relation(syms->i, clauses2, disabled, echo);
#            zap_ilist(syms);
#            syms = eliminable_relations(clauses2, equality);
#          }
#
#   2. Each recomputation called eliminable_relation() once per relation
#      symbol, and each of THOSE calls scanned every clause and every literal.
#      R symbols x L literals, per round, for R rounds: O(R^2 * L).
#
#   3. elim_relation() ran a given-clause loop in which `usable` grew by
#      plist_append(), which walks to the tail.  n appends to a list growing to
#      n is quadratic, once per round.  9,000,012 plist_append calls at 3,000
#      predicates, 22.4% of the profile.
#
#   And elim_relation() resolved the given clause against EVERY clause in
#   usable, when only clauses mentioning the symbol can possibly resolve on it.
#
# THE FIX
#   (2) One pass over the clauses decides, for every symbol at once, the two
#   facts eliminable_relation() was computing per symbol: the greatest number
#   of occurrences in any single clause, and whether any negative occurrence
#   has arguments that are not distinct variables.  O(L) instead of O(R * L),
#   with identical results because it is the same predicate.  The per-symbol
#   counts live in an I2accum, the hash-backed multiset added for PF2, so each
#   lookup is O(1).
#
#   (3) The given-clause queue and the `usable`, `without` and `eliminable`
#   lists all keep a tail pointer, so each append is O(1) instead of a walk.
#
#   And gen_given() returns immediately for a clause that does not contain the
#   symbol, and skips those in `usable`.  That is result-preserving, not an
#   approximation: resolve_on_symbol() requires a literal with the symbol in
#   BOTH clauses, and the clause-id assignment it does happens inside that same
#   test, so a skipped pair produced nothing before either.
#
#   What is NOT changed is (1).  Recomputing is not gratuitous -- eliminating
#   one relation can make another eliminable, or stop it being so -- and any
#   scheme that reuses the candidate list across rounds changes WHICH symbols
#   get eliminated and in what order.  So the loop still runs one round per
#   symbol, each round now O(L): quadratic rather than cubic.
#
# MEASURED, Prover9 startup, default settings
#
#       symbols      before        after      speedup
#        1,000        0.49 s       0.04 s       12x
#        2,000        3.84 s       0.15 s       26x
#        4,000       29.57 s       0.54 s       55x
#        8,000      263.04 s       2.49 s      106x
#       16,000      >300 s (CPU limit)  11.73 s    --
#
#   The before column multiplies by about eight for each doubling, which is
#   cubic; the after column by about four, which is quadratic.  The 16,000 row
#   is a floor, not a ratio: the old build was killed at a 300-second CPU
#   limit, so its true time is unknown and larger.
#
#   Worth noting: startup with predicate elimination WORKING is now faster than
#   startup with it switched off (11.73 s against 19.48 s at 16,000 symbols),
#   because eliminating predicates removes clauses, and everything downstream
#   then has less to index.
#
# WHAT STILL DOMINATES, after the fix
#   gprof at 8,000 symbols:
#
#       14.65%  i2accum_add_n        96,044,000 calls
#       14.34%  lookup_by_id        480,484,252 calls
#        6.49%  is_symbol           320,368,173 calls
#        5.93%  i2accum_probe       288,824,960 calls
#        3.71%  predicate_elimination        1 call   (6.15 s cumulative, was 82.33 s)
#        2.84%  eliminable_relations     8,001 calls  (was 37.58% at 3,000)
#
#   eliminable_relations has gone from 37.6% to 2.8%, but there are still 8,001
#   rounds -- one per symbol, which is point (1) above -- and each round calls
#   rsym_set_in_topforms() over every clause.  That is the remaining quadratic.
#   The lookup_by_id and str_ident volume is a separate matter, recorded as PF6
#   in ../FIXES.md: logic_term() identifies connectives by comparing symbol
#   NAMES rather than cached symnums, which is where 161 million str_ident
#   calls come from.
#
# USAGE
#   perf-predicate-elim.sh [BASELINE_BIN_DIR]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
new="$here/../../bin/prover9"
old="${1:-}"

[ -x "$new" ] || { echo "no prover9 at $new -- run make first" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

generate() {
  local n="$1" out="$2" i
  {
    echo 'set(quiet).'
    echo 'clear(print_initial_clauses).'
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
    137|152) echo ">300 s (CPU)" ;;
    *) printf '%.2f s' "$(echo "$t1 - $t0" | bc)" ;;
  esac
}

printf '\n%-12s %-16s %-14s %s\n' 'SYMBOLS' 'BEFORE' 'AFTER' 'SPEEDUP'
printf '%s\n' '----------------------------------------------------------------'
for n in 500 1000 2000 4000 8000; do
  generate "$n" "$tmp/in.$n"
  a="n/a"
  [ -n "$old" ] && [ -x "$old/prover9" ] && a=$(run "$old/prover9" "$tmp/in.$n")
  b=$(run "$new" "$tmp/in.$n")
  r=""
  [ "${a: -1}" = "s" ] && [ "${b: -1}" = "s" ] && [ "${a:0:1}" != ">" ] &&
    r=$(printf '%.0fx' "$(echo "${a% s} / ${b% s}" | bc -l)")
  printf '%-12s %-16s %-14s %s\n' "$((n * 2))" "$a" "$b" "$r"
done
printf '%s\n' '----------------------------------------------------------------'
cat <<'NOTE'
Counting distinct symbols as 2n: n predicates and n constants.

The before column should multiply by about eight for each doubling of the
input (cubic); the after column by about four (quadratic).  A ">300 s" entry
is a floor -- the run was killed at the CPU limit -- so no ratio is shown for
it.
NOTE
