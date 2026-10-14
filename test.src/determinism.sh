#!/bin/bash
# Is prover9/mace4 deterministic?  This script answers that, and exists because
# answering it carelessly cost a long detour.
#
# THE QUESTION
#   Run prover9 twice on the same input, with the same flags, on the same
#   machine.  Should the output be identical?  For a saturation prover with no
#   random selection enabled, yes -- the given-clause loop is a deterministic
#   function of the input.  So if two runs differ, either the prover has a bug
#   (uninitialised memory, a pointer-ordered container, an address-dependent
#   hash) or the comparison is wrong.
#
# WHAT WE FOUND
#   The comparison was wrong, twice, and then the remainder was the clock.
#
#   1. The first filter did not strip the banner line, which echoes the path of
#      the binary that produced it.  An A/B between two builds installed in
#      different directories reported that 336 of 336 runs differed.
#
#   2. The second filter matched "wall" case-sensitively and so missed
#      "Wall_clock=".  A run that took 0 seconds and one that took 1 second
#      then looked like a behavioural difference.  This is what produced the
#      appearance of nondeterminism in a problem -- queens3.in -- that is in
#      fact bit-identical across runs.  valgrind --tool=memcheck
#      --track-origins=yes on that same run reports 0 uninitialised-value
#      reports, 0 invalid reads and "0 errors from 0 contexts", and disabling
#      ASLR with setarch -R changes nothing, so there was never any
#      uninitialised data to find.  Both filters are now in one place,
#      normalize_output.sh, which this script uses.
#
#   3. What survives a correct filter is the clock, and only the clock.  Over
#      a 336-run corpus:
#
#        - 134 runs never terminate on their own.  They are open-ended
#          searches: mace4 climbing domain sizes with no upper bound, prover9
#          saturating a non-theorem.  The harness kills them at a CPU limit,
#          which lands mid-output, so they are cut at a different point each
#          time.  redeclare.in reached domain size 117 on one run and further
#          on the next.  That is the resource limit, not the prover.
#
#        - 202 runs do terminate on their own.  190 of those are identical
#          across repeated runs AND across two different builds.
#
#        - The remaining 12 all exit on a clock: nine mace4 runs with
#          "exit (max_sec_no)" and three prover9 runs with
#          "exit (max_seconds)".  They differ by the same amount when the SAME
#          binary is compared against itself, which is the control that settles
#          it -- a difference that appears in A/A is the measurement, not the
#          change under test.
#
#   Conclusion: given a fixed resource budget that the search does not reach,
#   prover9 and mace4 are deterministic.  Every difference we could produce was
#   either our filter or a search being stopped by a timer.
#
# WHY IT MATTERS
#   This is the control for every A/B in FIXES.md.  Without a trustworthy
#   "identical" test there is no way to claim that a change to the library did
#   not alter what the provers conclude.  The A/A column below is not
#   decoration: it is the noise floor, and an A/B result is only meaningful
#   when read against it.  Earlier figures in this project's history were
#   quoted without that control and were inflated by the two filter defects
#   above.
#
# USAGE
#   test.src/determinism.sh N
#       Run the default set -- the shipped prover9.examples and mace4.examples
#       -- N times each.  This is what `make tier2` runs, so the check is a
#       gate rather than a procedure described in a comment.  (determinism.sh
#       was NOT wired into any target for some time after it was written, which
#       is exactly the failure the tier3 scripts were created to fix; see the
#       tier comments in Makefile.)
#
#   test.src/determinism.sh N PROBLEM [PROBLEM...]
#       Run each named problem N times with ../bin/prover9 and ../bin/mace4 and
#       report whether all N normalised outputs agree.
#
#   test.src/determinism.sh -b OTHER_BIN_DIR N PROBLEM [PROBLEM...]
#       Also compare against the binaries in OTHER_BIN_DIR (an A/B), after
#       first establishing the A/A floor.  Use this when checking that a
#       library change left behaviour alone.
#
#   Exit status 0 when every self-terminating, non-clock-limited run agrees.

set -u

here=$(cd "$(dirname "$0")" && pwd)
norm="$here/normalize_output.sh"
bin="$here/../bin"
other=""

[ -x "$norm" ] || { echo "cannot execute $norm" >&2; exit 2; }

if [ "${1:-}" = "-b" ]; then
  other="$2"; shift 2
fi

n="${1:-3}"; shift || true

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# With no problems named, use a generated set plus the examples the tarball
# ships.  The generated ones are all SATISFIABLE and FINITELY SATURATING,
# which is the shape that terminates for both provers without needing
# prover-specific options: Mace4 finds a model, and Prover9 derives everything
# derivable and then exhausts its set of support.  Both stop for a reason the
# input determines, which is the only kind of run worth comparing.
#
# The shipped examples are kept as well, even though two of the three are
# resource-limited for Prover9 and get skipped: group2.in and rw1.in are real
# Mace4 searches, and x2.in is a real proof with hints.
if [ $# -eq 0 ]; then
  cat > "$tmp/d1.in" <<'EOF'
formulas(sos).
  p(a).
  q(b).
  p(x) -> r(x).
  r(x) & q(y) -> s(x,y).
end_of_list.
EOF
  cat > "$tmp/d2.in" <<'EOF'
formulas(sos).
  f(a) = b.
  f(b) = a.
  g(x) = f(f(x)).
  p(a).
end_of_list.
EOF
  cat > "$tmp/d3.in" <<'EOF'
formulas(sos).
  -big(x) | small(x).
  big(c1).
  big(c2).
  small(x) -> tiny(x) | medium(x).
end_of_list.
EOF
  cat > "$tmp/d4.in" <<'EOF'
formulas(sos).
  e(x,x).
  e(x,y) -> e(y,x).
  e(a,b).
  e(b,c).
end_of_list.
EOF
  set -- "$tmp/d1.in" "$tmp/d2.in" "$tmp/d3.in" "$tmp/d4.in" \
         $(ls "$here/../prover9.examples"/*.in "$here/../mace4.examples"/*.in \
             2>/dev/null)
  echo "using 4 generated satisfiable sets plus the shipped examples;"
  echo "pass paths to override"
fi

# A run that stops because a timer went off is not comparable: the cut lands
# wherever the clock happened to land.  Detect that from the prover's own exit
# banner rather than guessing from the wall time.
clock_limited() {
  grep -qaE 'exit \((max_seconds|max_sec_no|max_minutes|max_hours)\)' "$1"
}

# A run killed from outside has no exit banner at all -- its output is simply
# truncated.  Those are not comparable either.
self_terminating() {
  grep -qaE 'exit \(|Exiting|exit_code' "$1"
}

fail=0
printf '%-9s %-26s %-18s %s\n' PROG PROBLEM VERDICT NOTE
printf '%s\n' '---------------------------------------------------------------------------'

for prob in "$@"; do
  for prog in prover9 mace4; do
    [ -x "$bin/$prog" ] || continue

    # ---- A/A: the same binary, n times.  This is the noise floor. ----
    hashes=""
    for i in $(seq 1 "$n"); do
      # The outer braces exist to discard this shell's own "Killed" notice for
      # a run that hits the CPU limit.  Such a run is reported as SKIP below,
      # so the notice is noise -- and noise in a gate hides real failures.
      { ( ulimit -t 60; exec "$bin/$prog" -f "$prob" ) > "$tmp/raw" 2>&1 ; } 2>/dev/null
      "$norm" < "$tmp/raw" > "$tmp/run.$i"
      hashes="$hashes $(md5sum < "$tmp/run.$i" | cut -c1-10)"
    done

    note=""
    if ! self_terminating "$tmp/raw"; then
      verdict="SKIP"; note="killed at the resource limit, output truncated"
    elif clock_limited "$tmp/raw"; then
      verdict="SKIP"; note="stops on a timer; the cut point is the clock"
    elif [ "$(printf '%s\n' $hashes | sort -u | wc -l)" -eq 1 ]; then
      verdict="deterministic"; note="$n runs agree"
    else
      verdict="DIFFERS"; note="$n runs disagree: $hashes"; fail=1
    fi

    # ---- A/B: the other build, only where A/A was clean. ----
    if [ -n "$other" ] && [ "$verdict" = deterministic ] && [ -x "$other/$prog" ]; then
      { ( ulimit -t 60; "$other/$prog" -f "$prob" ) 2>&1 | "$norm" > "$tmp/other" ; } 2>/dev/null
      if cmp -s "$tmp/run.1" "$tmp/other"; then
        note="$note; matches $other"
      else
        verdict="A/B DIFFERS"; note="$note; differs from $other"; fail=1
      fi
    fi

    printf '%-9s %-26s %-18s %s\n' \
      "$prog" "$(basename "$prob")" "$verdict" "$note"
  done
done

printf '%s\n' '---------------------------------------------------------------------------'
if [ "$fail" = 0 ]; then
  echo "determinism.sh: ALL PASSED"
else
  echo "determinism.sh: FAILED"
fi
exit "$fail"
