#!/bin/bash
# Regression tests for the defects described in ../FIXES.md.
# Each one fails against pristine LADR-2009-11A and passes after the fixes.
#
#   usage: ./regress.sh [bin-directory]
BASE=$(cd "$(dirname "$0")/.." && pwd)
B="${1:-$BASE/bin}"
W=$(mktemp -d "${TMPDIR:-/tmp}/ladr-regress.XXXXXX")
trap 'rm -rf "$W"' EXIT

pass=0; fail=0
chk() {
  if eval "$2" >/dev/null 2>&1; then printf "  PASS  %s\n" "$1"; pass=$((pass+1))
  else                               printf "  FAIL  %s\n" "$1"; fail=$((fail+1)); fi
}

LONG=$(printf 'z%.0s' $(seq 1 300))
printf 'formulas(assumptions).\nP(a).\nend_of_list.\n'          > "$W/triv.in"
printf 'formulas(assumptions).\nx = x.\nend_of_list.\n'         > "$W/tiny.in"
printf 'f(%%s%%s%%s%%s%%s%%n,a)\n'                              > "$W/fmt.in"

# A proof file truncated immediately after a "%" comment line.
"$B/prover9" -f "$BASE/prover9.examples/x2.in" > "$W/proof.txt" 2>&1
awk '/= PROOF =/{p=1} {print} p&&/^% Proof/{exit}' "$W/proof.txt" > "$W/trunc.txt"

opt() {  # opt <assign-command>; prints prover9's output
  printf '%s\n' "$1" > "$W/o.in"
  timeout 30 "$B/prover9" -f "$W/o.in" "$W/triv.in" 2>&1
}

echo "Regression tests (see FIXES.md):"

# M1: sprintf into char str[20] in mace4's domain-size banner.
chk "M1  mace4 -n 10000000 (stack buffer overflow)" \
    'timeout 30 "$B/mace4" -n 10000000 -f "$W/tiny.in" 2>&1 | grep -q "DOMAIN SIZE 10000000"'

# M2/M3: sprintf of a long filename into char s[100].
chk "M2  prover9 long missing filename (overflow)" \
    'timeout 30 "$B/prover9" -f "$LONG" 2>&1 | grep -q "not found"'
chk "M3  prooftrans long missing filename (overflow)" \
    'timeout 30 "$B/prooftrans" -f "$LONG" 2>&1 | grep -q "not found"'

# M5: -label is formatted into a fixed char[100]; a long label overflows it
# (glibc fortify aborts).  The label must survive intact in the output.
chk "M5  prooftrans -label with a 300-char label (overflow)" \
    'timeout 30 "$B/prooftrans" hints -label "$LONG" < "$W/proof.txt" 2>/dev/null | grep -q "${LONG}_1"'

# F1: input text used as a printf format string (segfaults before the fix).
chk "F1  format directives in fastparse input (crash)" \
    'timeout 30 "$B/unfast" < "$W/fmt.in" >/dev/null 2>&1; [ $? -ne 139 ]'

# D1: truncated proof input (infinite loop, then NULL deref, before the fix).
chk "D1  prooftrans on truncated proof (hang)" \
    'timeout 30 "$B/prooftrans" < "$W/trunc.txt" >/dev/null 2>&1; [ $? -eq 1 ]'
chk "D1  directproof on truncated proof (hang)" \
    'timeout 30 "$B/directproof" < "$W/trunc.txt" >/dev/null 2>&1; [ $? -eq 1 ]'
chk "D1  prooftrans still handles a complete proof" \
    'timeout 30 "$B/prooftrans" parents_only < "$W/proof.txt" 2>&1 | grep -q "end of proof"'

# D1 side effect: the drain loop used to swallow the blank line after the
# comment block and print it back, so upstream emits two blank lines between
# a proof's comments and its first clause.  There must now be exactly one.
chk "D1  exactly one blank line after the proof comments" \
    'timeout 30 "$B/prooftrans" < "$W/proof.txt" 2>/dev/null \
       | awk "/^% Given clauses/{g=NR} g&&NR>g&&/^\$/{n++} g&&NR>g&&!/^\$/{exit} END{exit !(n==1)}"'

# D2: fscanf("%s") carried no field width, so a token of unbounded length
# from the input was read into char s[1000].  A proof whose clause id is a
# long run of non-whitespace overflowed it (ASan: stack-buffer-overflow,
# WRITE of size 5001 at prooftrans.c:379); at this length the plain build
# segfaults.  Both tools must now reject the input cleanly instead.
awk 'BEGIN{s="";while(length(s)<200000)s=s "xxxxxxxxxx";print substr(s,1,200000)}' \
    > "$W/tok.txt"
awk -v tokfile="$W/tok.txt" '
  BEGIN{getline tok < tokfile}
  /= PROOF =/{p=1}
  p && !done && /^[0-9]+ /{sub(/^[0-9]+/, tok); done=1}
  {print}' "$W/proof.txt" > "$W/longtok.txt"

chk "D2  prooftrans on an over-long clause id (overflow)" \
    'timeout 30 "$B/prooftrans" < "$W/longtok.txt" >/dev/null 2>&1; [ $? -eq 1 ]'
chk "D2  directproof on an over-long clause id (overflow)" \
    'timeout 30 "$B/directproof" < "$W/longtok.txt" >/dev/null 2>&1; [ $? -eq 1 ]'

# N1/N2/N3: derived parm values that used to abort the program.
chk "N1  assign(max_minutes,-1) accepted" \
    '! opt "assign(max_minutes,-1)." | grep -q "Fatal error"'
chk "N1  assign(max_hours,-1) accepted" \
    '! opt "assign(max_hours,-1)." | grep -q "Fatal error"'
chk "N1  assign(max_days,-1) accepted" \
    '! opt "assign(max_days,-1)." | grep -q "Fatal error"'
chk "N2  assign(max_minutes,50000000) saturates" \
    'opt "assign(max_minutes,50000000)." | grep -q "max_seconds, 2147483647"'
chk "N2  assign(max_hours,1000000) saturates" \
    'opt "assign(max_hours,1000000)." | grep -q "max_seconds, 2147483647"'
chk "N3  assign(pick_given_ratio,-1) accepted" \
    '! opt "assign(pick_given_ratio,-1)." | grep -q "Fatal error"'
# ...without changing in-range derivations:
chk "N   assign(max_minutes,2) still derives 120" \
    'opt "assign(max_minutes,2)." | grep -q "max_seconds, 120"'
chk "N   assign(max_hours,3) still derives 10800" \
    'opt "assign(max_hours,3)." | grep -q "max_seconds, 10800"'
chk "N   assign(pick_given_ratio,4) still derives 4" \
    'opt "assign(pick_given_ratio,4)." | grep -q "weight_part, 4"'

# U1: signed overflow in init_giv_select on a default run.  Only a UBSan or
# -ftrapv build can observe this, so just check prover9 still runs normally.
chk "U1  default prover9 run completes" \
    'timeout 30 "$B/prover9" -f "$BASE/prover9.examples/x2.in" 2>&1 | grep -q "end of proof"'

# X1-X5: the installed helper scripts.  These need the binaries on PATH.
export PATH="$B:$PATH"
printf 'formulas(assumptions).\n(x * y) * z = x * (y * z).\nend_of_list.\n' > "$W/head.in"
printf 'x * y = y * x.\n%% comment\nx * x = x.\n'                           > "$W/cands.txt"
"$B/prover9" -f "$BASE/prover9.examples/x2.in" 2>&1 \
  | "$B/prooftrans" xml > "$W/p.xml" 2>/dev/null

chk "X   looper (mace4) runs and reports a model" \
    'cd "$W" && timeout 90 "$B/looper" mace4 assumptions head.in < cands.txt 2>&1 | grep -q "MODEL"'
chk "X   looper (prover9) runs and reports a proof" \
    'cd "$W" && timeout 90 "$B/looper" prover9 goals head.in < cands.txt 2>&1 | grep -qE "PROOF|Problem"'
chk "X   attack runs and writes its interps file" \
    'cd "$W" && rm -f interps && timeout 120 "$B/attack" head.in interps < cands.txt 2>&1 | grep -q "checked" && [ -s interps ]'
chk "X5  gvizify is executable and emits graphviz dot" \
    '[ -x "$B/gvizify" ] && timeout 60 "$B/gvizify" "$W/p.xml" 2>/dev/null | grep -q "strict digraph"'
chk "X   prover9-mace4 races both and returns the winner" \
    'timeout 90 "$B/prover9-mace4" -f "$BASE/prover9.examples/x2.in" 2>/dev/null | grep -q "THEOREM PROVED"'

echo
if [ $fail -eq 0 ]; then echo "ALL $pass REGRESSION TESTS PASSED"; else
  echo "$fail of $((pass+fail)) REGRESSION TESTS FAILED"; fi
exit $(( fail != 0 ))
