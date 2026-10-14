#!/bin/bash
#------------------------------------------------------------------------------
# Regression test for arity_check() / collect_multiples() in ladr/symbols.c.
#
# WHAT IT CHECKS, AND WHY A SHELL TEST
#   collect_multiples() finds the symbol names used with more than one arity.
#   It used to do that by comparing every symbol against every other:
#
#       for (p1 = syms; p1; p1 = p1->next) {
#         char *s1 = sn_to_str(p1->i);
#         for (p2 = p1->next; p2; p2 = p2->next)
#           if (str_ident(s1, sn_to_str(p2->i))) { ... }
#       }
#
#   -- O(n^2) string comparisons over every symbol in the input.  On
#   Problems/CSR/CSR040-10.p that was 239,225,001 of the 263,958,220 str_ident
#   calls in the profile.  It is grouped by name now, which is O(n log n).
#
#   The result feeds straight into a fatal_input_error message that names the
#   symbols IN LIST ORDER, so the order of the list is user-visible output and
#   not an internal detail.  It is also an order nobody would guess: the
#   pairwise loop discovered groups in order of each group's first member,
#   listed each group's members in input order, and prepended every symbol,
#   which reverses the whole thing.
#
#   The corpus A/B cannot protect any of that.  This is a fatal error on
#   malformed input, so no well-formed problem reaches it -- which is exactly
#   the situation where a test has to be written deliberately.  The strings
#   below were produced by the pairwise implementation, before the change.
#
# USAGE
#   arity_check_test.sh [bin-directory]
#------------------------------------------------------------------------------

set -u
here=$(cd "$(dirname "$0")" && pwd)
bin="${1:-$here/../bin}"
p9="$bin/prover9"

[ -x "$p9" ] || { echo "no prover9 at $p9" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

failures=0

# Run prover9 and return just the list from the "multiple arities" message, or
# the word NONE if it did not complain.
arity_message() {
  local f="$1" line
  line=$("$p9" -f "$f" 2>&1 | grep -aoE 'multiple arities: [^.]*' | head -1)
  if [ -z "$line" ]; then
    echo "NONE"
  else
    echo "${line#multiple arities: }"
  fi
}

check() {
  local what="$1" f="$2" want="$3" got
  got=$(arity_message "$f")
  if [ "$got" = "$want" ]; then
    printf 'ok:   %s\n' "$what"
  else
    printf 'FAIL: %s\n      expected: %s\n      got:      %s\n' "$what" "$want" "$got"
    failures=$((failures + 1))
  fi
}

# 1. Several groups, interleaved, so the output order is not simply the input
#    order.  This is the case that pins the ordering rules.
cat > "$tmp/many.in" <<'EOF'
formulas(sos).
  p(a) & q(a,b).
  p(a,b) & r(c).
  s(d) & q(e).
  r(c,d,e) & p(a,b,c).
  t(f) & s(g,h).
end_of_list.
EOF
check "several interleaved groups, in the original order" "$tmp/many.in" \
  "s/2, s/1, r/3, r/1, q/1, q/2, p/3, p/2, p/1"

# 2. A single name with two arities.
cat > "$tmp/one.in" <<'EOF'
formulas(sos).
  p(a).
  p(a,b).
end_of_list.
EOF
check "one name with two arities" "$tmp/one.in" "p/2, p/1"

# 3. A name with three arities: the whole group, in input order, reversed.
cat > "$tmp/three.in" <<'EOF'
formulas(sos).
  p(a).
  p(a,b).
  p(a,b,c).
end_of_list.
EOF
check "one name with three arities" "$tmp/three.in" "p/3, p/2, p/1"

# 4. Nothing wrong: no message at all.  The grouping code has an early return
#    for this, so it is worth checking that it returns nothing rather than an
#    empty list that still triggers the message.
cat > "$tmp/clean.in" <<'EOF'
formulas(sos).
  p(a) & q(b,c) & r(d,e,f).
end_of_list.
EOF
check "no multiple arities, no complaint" "$tmp/clean.in" "NONE"

# 5. A function symbol and a relation symbol sharing a name.  arity_check
#    concatenates fsyms and rsyms, so this crosses that boundary.
cat > "$tmp/mixed.in" <<'EOF'
formulas(sos).
  p(f(a)).
  f(a,b) = c.
end_of_list.
EOF
check "a name shared across function and relation symbols" "$tmp/mixed.in" "f/2, f/1"

# 6. Many groups rather than one big one.  This is the path where the groups
#    have to be ordered by first appearance, and where a careless rewrite
#    reintroduces a scan per group -- which is how the first version of the
#    grouped implementation went wrong before measurement caught it.
cat > "$tmp/groups.in" <<'EOF'
formulas(sos).
  p0(a). p0(a,b).
  p1(a). p1(a,b).
  p2(a). p2(a,b).
  p3(a). p3(a,b).
end_of_list.
EOF
check "four separate groups, ordered by first appearance" "$tmp/groups.in" \
  "p3/2, p3/1, p2/2, p2/1, p1/2, p1/1, p0/2, p0/1"

# 7. One symbol only: the n < 2 early return.
cat > "$tmp/single.in" <<'EOF'
formulas(sos).
  p.
end_of_list.
EOF
check "a single symbol, nothing to compare" "$tmp/single.in" "NONE"

echo
if [ "$failures" = 0 ]; then
  echo "arity_check_test: ALL PASSED"
else
  echo "arity_check_test: FAILED ($failures failure(s))"
fi
exit "$failures"
