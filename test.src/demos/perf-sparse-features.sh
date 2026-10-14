#!/bin/bash
#------------------------------------------------------------------------------
# A measurement, not a pass/fail test: how long Prover9 takes to start up on a
# file with n distinct predicates.  max_given=0 stops the loop after the first
# given clause, so this is startup alone.
#
# THE DEFECT
#   Subsumption is expensive, so Prover9 filters candidates first.  Each clause
#   gets a vector of small non-negative integers chosen so that
#
#       if C subsumes D  then  feature(C) <= feature(D)  componentwise
#
#   and the index in ladr/di_tree.c retrieves exactly the clauses passing that
#   test; only those go on to a real subsumption check.
#
#   The vector was DENSE and positional:
#
#       [ positive literals, negative literals,
#         then for each relation symbol:  pos occ, neg occ,
#         then for each function symbol:  pos occ, neg occ, pos max, neg max ]
#
#   -- so its LENGTH was a property of the knowledge base while its CONTENT was
#   a property of the clause.  A clause mentioning five symbols still got an
#   entry for every symbol in the input, nearly all of them zero, built as a
#   linked list of one-integer cells.  And the index was a trie whose DEPTH was
#   the position, so every clause's path spelled out its zeros too.
#
#   On Problems/CSR/CSR040-10.p -- 5.7 MB of TPTP, 44,227 clauses and 7,161
#   distinct symbols after translation -- that is about 28,600 entries and
#   28,600 trie levels per clause.  Measured: 37,537 calls to features()
#   produced 3.3 billion get_ilist/get_mem/free_mem cycles, around 27% of the
#   whole run.
#
#   The sharpest form of the problem: on that same input the index those
#   vectors feed is EMPTY.  di_tree_forward was called 37,537 times and cost
#   0.00 seconds, on vectors that took a quarter of the run to build and throw
#   away.
#
# THE FIX
#   The vector holds only its non-zero entries, as (position, value) pairs in
#   increasing position order.  A clause mentioning k symbols has at most
#   2 + 4k pairs instead of 2 + 4R.
#
#   An absent position means zero, and that is what keeps the filter exact.
#   Every entry is non-negative, so
#
#       feature(C) <= feature(D) componentwise
#         iff  for every position p where C is non-zero, C[p] <= D[p]
#
#   because a position where C is zero constrains nothing.  The index nodes
#   carry their position explicitly, paths are as long as the clause is wide,
#   and a node can hold both data and children -- vectors no longer all have
#   the same length, so one clause's path can end where another's continues.
#
# WHAT DID AND DID NOT CHANGE IN THE OUTPUT
#   The set of clauses retrieved is identical, so the same clauses are
#   subsumed and the same proofs are found.  On quot-comm.in, for instance,
#   Given, Generated, Kept, Forward_subsumed=1154 and Back_subsumed=3 are all
#   unchanged.
#
#   Two things do change, and neither is a result:
#
#     - The filter enumerates candidates in a different order, so forward
#       subsumption reaches a subsumer after fewer tries.  On 6 of the 174
#       comparable corpus runs the Nonunit_fsub_feature_tests statistic falls
#       -- always falls, never rises:
#
#           quot-comm.in   5367 -> 4694      SET668+3.in    2053 -> 1996
#           GEO147+1.in    8080 -> 7901      wang-eq.in     5392 -> 5349
#           SET587+3.in    3194 -> 3102      HWV006-1.in      44 -> 42
#
#     - struct di_tree grew from 24 to 32 bytes (a position as well as a
#       label, and data and kids rather than a union of the two), so the
#       memory report prints "di_tree (  32)".  The node COUNT drops by far
#       more than the size rises, because paths are no longer padded with
#       zeros.
#
# MEASURED
#
#   Startup on CSR040-10 (44,227 clauses, 7,161 distinct symbols):
#
#       pristine upstream        167.92 s
#       dense feature vector      97.55 s
#       sparse feature vector     31.81 s      3.1x over dense, 5.3x over 2009
#
#   Problems that use the index heavily are unaffected, as expected -- their
#   vocabularies are small, so dense and sparse are the same size:
#
#       andrews.in         0.23 s -> 0.19 s
#       quot-general.in    0.09 s -> 0.10 s
#       GEO147+1.in        0.03 s -> 0.04 s
#
#   The synthetic ladder below is n non-unit clauses over 3n distinct symbols,
#   with a cap high enough that neither build runs out (assign(max_megs,
#   12000)), so that the two are doing the same work.  Peak RSS and elapsed:
#
#       symbols      dense              sparse
#        3,000        142 MB   6.24 s     5 MB   0.12 s
#        6,000        557 MB  50.63 s     7 MB   0.47 s
#       12,000       2211 MB 490.40 s    12 MB   2.11 s
#
#   Each doubling costs the dense build about four times the time and four
#   times the memory; the sparse build about twice the time and a little more
#   memory.  At 12,000 symbols that is 184x less memory and 232x less time.
#
#   IT IS A CAPABILITY DIFFERENCE, NOT ONLY A SPEED ONE.  Prover9's default
#   max_megs is 500 (provers.src/search.c, init_parm("max_megs", 500, ...) --
#   note that this is the prover's own parameter, not the library's
#   DEFAULT_MAX_MEGS, which it overrides).  At that default the dense build
#   cannot finish indexing 2,000 such clauses:
#
#       cap      dense                      sparse
#       500 MB   exit (max_megs), 1732 of   exit (max_given), 2000 of 2000
#                2000 clauses indexed
#      1000 MB   exit (max_given), 2000     exit (max_given), 2000 of 2000
#
#   So on a vocabulary of a few thousand symbols the dense vector turned a
#   completed index into a truncated one, at the shipped default.
#
# WHERE THE TIME GOES NOW
#   gprof on CSR040-10 after the change: the feature vector is off the profile
#   entirely, and 51% of what remains is ilist_cat called 22,512 times from
#   eq_defs() in provers.src/unfold.c, which accumulates per-symbol dependent
#   lists by walking to the tail.  Recorded as PF7 in ../FIXES.md.
#
# USAGE
#   perf-sparse-features.sh [BASELINE_BIN_DIR]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
new="$here/../../bin/prover9"
old="${1:-}"

[ -x "$new" ] || { echo "no prover9 at $new -- run make first" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# n distinct predicates, each in a NON-UNIT clause, so the nonunit feature
# index is actually built -- a file of units would exercise the unit
# discrimination index instead and say nothing about this change.
generate() {
  local n="$1" out="$2" cap="${3:-}" i
  {
    echo 'set(quiet).'
    echo 'clear(print_initial_clauses).'
    echo 'clear(predicate_elim).'
    echo 'assign(max_given, 0).'
    [ -n "$cap" ] && echo "assign(max_megs, $cap)."
    echo 'formulas(sos).'
    for ((i = 0; i < n; i++)); do
      echo "p$i(c$i) | q$i(c$i)."
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

# Time AND peak memory AND the outcome, because at the shipped default the
# dense build does not finish and a bare ratio of two timings would be
# comparing a completed index against a truncated one.
measure() {
  local bin="$1" f="$2" out
  out=$(/usr/bin/time -f '%M %e' sh -c "ulimit -t 600; '$bin' -f '$f' >/dev/null 2>&1" 2>&1 | tail -1)
  set -- $out
  printf '%6s MB %8s s' "$(( ${1:-0} / 1024 ))" "${2:-?}"
}

outcome() {
  local bin="$1" f="$2"
  ( ulimit -t 600; "$bin" -f "$f" ) 2>&1 |
    grep -aoE 'exit \([a-z_]+\)' | tail -1
}

printf '\n%-10s %-26s %-26s\n' 'SYMBOLS' 'DENSE' 'SPARSE'
printf '%s\n' '--------------------------------------------------------------------'
for n in 500 1000 2000; do
  generate "$n" "$tmp/in.$n" 12000
  a="            n/a          "
  [ -n "$old" ] && [ -x "$old/prover9" ] && a=$(measure "$old/prover9" "$tmp/in.$n")
  b=$(measure "$new" "$tmp/in.$n")
  printf '%-10s %-26s %-26s\n' "$((n * 3))" "$a" "$b"
done
printf '%s\n' '--------------------------------------------------------------------'
echo "(cap raised to 12000 MB so that both builds do the same work)"
echo
echo "At Prover9's shipped default of 500 MB, 2000 clauses / 6000 symbols:"
generate 2000 "$tmp/cap.in" 500
if [ -n "$old" ] && [ -x "$old/prover9" ]; then
  printf '  dense   %s\n' "$(outcome "$old/prover9" "$tmp/cap.in")"
fi
printf '  sparse  %s\n' "$(outcome "$new" "$tmp/cap.in")"
cat <<'NOTE'

Counting distinct symbols as 3n: n p-predicates, n q-predicates, n constants.

Each doubling costs the dense build about four times the time and four times
the memory -- the product of the clause count and the symbol count.  The
sparse build pays about twice the time, because each clause's vector is sized
by the clause.  An "exit (max_megs)" above means the index was never finished.
NOTE
