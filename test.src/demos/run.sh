#!/bin/bash
# Run the demonstrations and check each against the expectation stated in its
# own comment header.
#
#   usage: run.sh [bin-directory]
#
# This is deliberately self-checking rather than merely informational.  A
# demonstration whose header claims one thing while the file does another is
# worse than no demonstration, and these headers are meant to be quotable.
#
# Pointing this at a build of the original 2009 code is the interesting case:
# the defect demonstrations should FAIL there, which is what makes them
# demonstrations of anything.  Each header says what to expect.

BASE=$(cd "$(dirname "$0")/../.." && pwd)
D="$BASE/test.src/demos"
B="${1:-$BASE/bin}"
W=$(mktemp -d "${TMPDIR:-/tmp}/ladr-demos.XXXXXX")
trap 'rm -rf "$W"' EXIT
printf 'assign(max_seconds,20).\n' > "$W/opt"

pass=0; fail=0
verdict() {   # verdict <tptp file>; prints PROVED / FAILED / NO-TRANSLATION
  if ! ( cd "$D" && timeout 60 "$B/tptp_to_ladr" < "$1" ) > "$W/in" 2>/dev/null; then
    echo NO-TRANSLATION; return
  fi
  if timeout 90 "$B/prover9" -f "$W/opt" "$W/in" 2>&1 | grep -q 'THEOREM PROVED'
  then echo PROVED; else echo FAILED; fi
}
expect() {  # expect <file> <wanted verdict> <what it demonstrates>
  local got; got=$(verdict "$1")
  if [ "$got" = "$2" ]; then
    pass=$((pass+1)); printf '  ok    %-28s %s\n' "$(basename "$1")" "$3"
  else
    fail=$((fail+1)); printf '  WRONG %-28s got %s, header says %s\n' \
      "$(basename "$1")" "$got" "$2"
  fi
}

echo "Demonstrations (headers in each file explain the defect):"

# Proved only if the negation of the quantified formula survived translation.
expect t4-negated-quantifier.p     PROVED "negation of a quantifier is kept"
# Proved only if the implicit pairwise inequality of distinct objects is stated.
expect t6-distinct-objects.p       PROVED "distinct objects are pairwise unequal"
# Must NOT be proved: a distinct object may still equal an ordinary term.
expect t6b-distinct-vs-ordinary.p  FAILED "...but may equal an ordinary term"
# Proved only if ~& and ~| parse and mean NAND and NOR.
expect t8-nand-nor.p               PROVED "NAND and NOR are understood"
# Proved only if the single-quoted formula name was re-quoted for LADR.
expect t5-quoted-formula-name.p    PROVED "quoted formula names are readable"
# Must NOT be proved: the excluded axiom must stay excluded.
expect t7-selective-include.p      FAILED "selective include omits the rest"

echo
echo "The memory-safety and performance demonstrations need a special build or"
echo "a measurement, so they carry their own instructions:"
for f in "$D"/*.in "$D"/*.sh; do
  case "$(basename "$f")" in run.sh) continue ;; esac
  [ -e "$f" ] || continue
  printf '  see  %s\n' "$(basename "$f")"
done

echo
if [ "$fail" -eq 0 ]; then echo "ALL $pass DEMONSTRATIONS BEHAVE AS DOCUMENTED"
else echo "$fail of $((pass+fail)) DEMONSTRATIONS DISAGREE WITH THEIR HEADERS"; fi
exit $(( fail != 0 ))
