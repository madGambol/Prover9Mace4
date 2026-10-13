#!/bin/bash
# Semantic check of the TPTP -> LADR translation.
#
# Every case below states a TPTP formula and the LADR formula that means the
# same thing, and the check is a PROOF of their equivalence by prover9 rather
# than a comparison of text.  That matters: the translator's output is
# formatted its own way, so a textual expectation either has to track the
# printer or be written loosely enough to miss real errors.  Asking the prover
# settles it.
#
# This found two defects that a textual test would not have.  The dropped
# negation of a quantified formula (FIXES.md T4) printed a formula that looked
# perfectly reasonable -- it was simply the opposite of the input.  And "~&"
# and "~|" produced no output at all to compare against, because the parser
# had never been told the operators exist (T8).
#
# For each pair <TPTP formula> <LADR formula meaning the same thing>, translate
# the TPTP and then ask prover9 to prove the two equivalent.  A textual
# comparison can be fooled by an equivalent-but-differently-printed form; a
# proof of equivalence cannot.
#
#   usage: semcheck.sh <bin-dir>
B="${1:-$(cd "$(dirname "$0")/.." && pwd)/bin}"
pass=0; fail=0

chk() {  # chk <tptp formula> <ladr formula with the same meaning>
  local tptp="$1" want="$2" t got
  t=$(mktemp -d)
  printf 'fof(t,axiom, %s ).\n' "$tptp" > "$t/p"
  if ! "$B/tptp_to_ladr" < "$t/p" > "$t/raw" 2>/dev/null; then
    printf '  XLATE-FAIL  %s\n' "$tptp"; fail=$((fail+1)); rm -rf "$t"; return
  fi
  got=$(sed -e 's/ # label([^)]*)//g' "$t/raw" \
        | grep -vE '^(set|formulas|end_of_list|%|$)' | sed 's/\.$//' | tr -d '\n')
  if [ -z "$got" ]; then
    printf '  EMPTY       %s\n' "$tptp"; fail=$((fail+1)); rm -rf "$t"; return
  fi
  printf 'set(prolog_style_variables).\nassign(max_seconds,10).\nformulas(goals).\n(%s) <-> (%s).\nend_of_list.\n' \
      "$got" "$want" > "$t/eq"
  if "$B/prover9" -f "$t/eq" 2>&1 | grep -q 'THEOREM PROVED'; then
    pass=$((pass+1)); printf '  ok     %s\n' "$tptp"
  else
    fail=$((fail+1)); printf '  WRONG  %-28s -> %s   (want %s)\n' "$tptp" "$got" "$want"
  fi
  rm -rf "$t"
}

# negation against quantifiers -- the T4 family
chk '~ ! [X] : p(X)'                 '-(all X p(X))'
chk '~ ? [X] : p(X)'                 '-(exists X p(X))'
chk '~ ~ ! [X] : p(X)'               '(all X p(X))'
chk '~ ! [X] : ~ p(X)'               '-(all X -p(X))'
chk '! [X] : ~ ? [Y] : q(X,Y)'       '(all X -(exists Y q(X,Y)))'
chk '? [X] : ~ ! [Y] : q(X,Y)'       '(exists X -(all Y q(X,Y)))'
chk '! [X,Y] : ~ p(X,Y)'             '(all X all Y -p(X,Y))'
chk '~ ! [X,Y] : p(X,Y)'             '-(all X all Y p(X,Y))'
# negation reaching a quantifier through a binary connective
chk 'q(a) => ~ ! [X] : p(X)'         'q(a) -> -(all X p(X))'
chk 'q(a) & ~ ? [X] : p(X)'          'q(a) & -(exists X p(X))'
chk 'q(a) | ~ ! [X] : p(X)'          'q(a) | -(all X p(X))'
chk 'q(a) <=> ~ ! [X] : p(X)'        'q(a) <-> -(all X p(X))'
# the compound negated connectives
chk 'p(a) ~& q(b)'                   '-(p(a) & q(b))'
chk 'p(a) ~| q(b)'                   '-(p(a) | q(b))'
chk 'p(a) <~> q(b)'                  '-(p(a) <-> q(b))'
chk '~ (p(a) ~& q(b))'               '(p(a) & q(b))'
# implication direction
chk 'p(a) <= q(b)'                   'q(b) -> p(a)'
chk 'p(a) => q(b)'                   'p(a) -> q(b)'
chk 'p(a) & q(b) & r(c)'             'p(a) & q(b) & r(c)'
chk 'p(a) | q(b) | r(c)'             'p(a) | q(b) | r(c)'
# equality, inequality, truth constants
chk 'a = b'                          'a = b'
chk 'a != b'                         '-(a = b)'
chk '~ a = b'                        '-(a = b)'
chk '$true'                          '$T'
chk '$false'                         '$F'
chk '! [X] : X = X'                  '(all X X = X)'

echo "  ---- pass $pass  fail $fail"
[ "$fail" -eq 0 ]
