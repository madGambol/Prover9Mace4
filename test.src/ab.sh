#!/bin/bash
#
# ab.sh -- the behaviour A/B: run every corpus pair under two builds and
#          report only the pairs whose output differs.
#
# ---------------------------------------------------------------------------
# WHY THIS GATE EXISTS, AND WHY IT IS A SCRIPT AND NOT A PARAGRAPH
#
# Tier2 is 41 problems of a few dozen clauses.  It passed green on a build
# that had silently stopped proving SWC258+1, SET667+3 and olsax2, because a
# change to the discrimination tree's child ordering broke two readers.  No
# unit test could see it: the invariant it broke only matters at a scale and
# a code path that small inputs never reach.  This comparison is what found
# it, and for five consecutive merges it existed only as a shell snippet in a
# README, which meant it was skipped five times.
#
# ---------------------------------------------------------------------------
# WHY CLOCK-LIMITED RUNS ARE SKIPPED AT RUN TIME, NOT FROM A LIST
#
# A run that ends "exit (max_seconds)" stopped wherever the clock happened to
# land, so two such runs differ for reasons that have nothing to do with the
# code.  Comparing them has twice sent this project chasing prover
# nondeterminism that did not exist, and once produced a false "serious
# regression" that was two runs landing either side of a 60-second limit on
# 61-second work.
#
# comparable.txt was meant to hold only self-terminating pairs, and was built
# by running each one and keeping those that did not exit on a timer.  That
# cannot work as a static list: whether a problem finishes inside its limit
# depends on how loaded the machine is.  Derived on an idle machine,
# SYN551+2 and SET667+3 were self-terminating; re-run while a 64-job sweep
# was in flight, both exit (max_seconds), and both then differ against
# THEMSELVES -- an A/A difference, with no second build involved.
#
# So the rule is applied where the fact is actually known: after the run.  If
# either side stopped on a clock, the pair is reported "skip" and excluded.
# A memory ceiling is NOT a clock -- "exit (max_megs)" is determined by the
# input and is exactly the kind of difference this gate should catch.
#
# ---------------------------------------------------------------------------
# USAGE
#
#   ./ab.sh aa                 # the noise floor: build A against itself
#   ./ab.sh ab BASELINE=...    # the comparison
#
#   A=<bin dir> B=<bin dir> CORPUS=<pairs file> JOBS=<n> ./ab.sh {aa|ab}
#
# Run aa FIRST.  An ab number means nothing without the floor, which should
# be zero; anything else means the corpus list or the machine is the problem,
# not the code.
# ---------------------------------------------------------------------------

mode=${1:-ab}
A=${A:-../bin}
B=${B:-}
CORPUS=${CORPUS:-$HOME/Projects/prover9-corpus/comparable.txt}
JOBS=${JOBS:-12}
CPU=${CPU:-600}
NORM=$(cd "$(dirname "$0")" && pwd)/normalize_output.sh

if [ "$mode" != aa ] && [ "$mode" != ab ]; then
  echo "usage: $0 {aa|ab}   (see the comments in this file)" >&2
  exit 2
fi
if [ "$mode" = ab ] && [ -z "$B" ]; then
  echo "$0 ab needs B=<bin dir> to compare against A=$A" >&2
  exit 2
fi
if [ ! -r "$CORPUS" ]; then
  echo "$0: cannot read corpus list $CORPUS" >&2
  exit 2
fi

# A run that stopped on a clock is not comparable.  max_megs is not a clock.
clock_limited() {
  printf '%s' "$1" | grep -qE 'exit \((max_seconds|max_sec_no|max_sec_yes|max_domain_seconds)\)'
}

one_pair() {
  pair=$1; mode=$2; A=$3; B=$4; NORM=$5; CPU=$6
  prog="${pair%%|*}"; path="${pair#*|}"
  case $mode in aa) x=$A; y=$A ;; ab) x=$A; y=$B ;; esac

  raw1=$( ulimit -t "$CPU"; "$x/$prog" -f "$path" 2>&1 )
  raw2=$( ulimit -t "$CPU"; "$y/$prog" -f "$path" 2>&1 )

  if printf '%s' "$raw1" | grep -qE 'exit \((max_seconds|max_sec_no|max_sec_yes|max_domain_seconds)\)' \
     || printf '%s' "$raw2" | grep -qE 'exit \((max_seconds|max_sec_no|max_sec_yes|max_domain_seconds)\)'; then
    echo "skip $prog $path (stopped on a clock)"
    return
  fi

  o1=$(printf '%s' "$raw1" | $NORM)
  o2=$(printf '%s' "$raw2" | $NORM)
  if [ "$o1" = "$o2" ]; then echo "same $prog $path"; else echo "DIFF $prog $path"; fi
}
export -f one_pair

out=$(mktemp) || exit 2
trap 'rm -f "$out"' 0 1 2 3 15

xargs -a "$CORPUS" -d'\n' -P "$JOBS" -I{} \
  bash -c 'one_pair "$1" "$2" "$3" "$4" "$5" "$6"' _ "{}" "$mode" "$A" "$B" "$NORM" "$CPU" > "$out" 2>&1

n_same=$(grep -c '^same' "$out")
n_diff=$(grep -c '^DIFF' "$out")
n_skip=$(grep -c '^skip' "$out")

echo "--- $mode: $n_same same, $n_diff differing, $n_skip skipped (clock-limited) ---"
grep '^DIFF' "$out"
grep '^skip' "$out" | sed 's/^/    /'

if [ "$n_diff" -eq 0 ]; then
  echo "ok:   no behavioural difference over $n_same comparable pairs"
  exit 0
else
  echo "$n_diff pair(s) differ -- each needs explaining, not averaging"
  exit 1
fi
