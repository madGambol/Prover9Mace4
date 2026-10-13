#!/bin/bash
# Regression tests for the defects described in ../FIXES.md.
# Each one fails against pristine LADR-2009-11A and passes after the fixes.
#
#   usage: ./regress.sh [bin-directory]
BASE=$(cd "$(dirname "$0")/.." && pwd)
B="${1:-$BASE/bin}"
W=$(mktemp -d "${TMPDIR:-/tmp}/ladr-regress.XXXXXX")
trap 'rm -rf "$W"' EXIT

pass=0; fail=0; skip=0
chk() {
  if eval "$2" >/dev/null 2>&1; then printf "  PASS  %s\n" "$1"; pass=$((pass+1))
  else                               printf "  FAIL  %s\n" "$1"; fail=$((fail+1)); fi
}
# For defects that only a sanitizer can observe: a plain build cannot tell
# whether they are present, so report SKIP rather than a meaningless PASS.
chk_san() {
  if nm -u "$B/prover9" 2>/dev/null | grep -q "__asan"; then chk "$1" "$2"
  else printf "  SKIP  %s (needs a sanitizer build)\n" "$1"; skip=$((skip+1)); fi
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

# M7: dollar.c sized Op_codes from the symbol count, then filled it using
# str_to_sn() calls that create symbols and so push the count past the end.
# set(production) implies set(eval_rewrite), the only caller.  Nothing but a
# sanitizer sees this -- a plain build and MALLOC_CHECK_=3 both run clean.
printf 'set(production).\nassign(max_seconds,5).\nformulas(sos).\n  p(1 + 1).\n  q(max(3,5)).\nend_of_list.\n' \
    > "$W/prod.in"
chk_san "M7  production mode heap overflow in dollar.c" \
    '! ASAN_OPTIONS=detect_leaks=0 timeout 60 "$B/prover9" -f "$W/prod.in" 2>&1 \
       | grep -q "AddressSanitizer"'

# T1: tptp_to_ladr formatted a missing include() filename into char s[100],
# so a TPTP problem whose include() names a long file aborted the translator
# (glibc: "*** buffer overflow detected ***", exit 134).  The filename comes
# straight from the input file, so this is reachable from any TPTP problem.
# Nothing upstream exercises either TPTP translator, hence this test.
printf "include('%s').\nfof(a,axiom,p(x)).\n" "$LONG" > "$W/longinc.p"
chk "T1  tptp_to_ladr long include() filename (overflow)" \
    'timeout 30 "$B/tptp_to_ladr" < "$W/longinc.p" 2>&1 | grep -q "not found"'

# T2: the messages that name a file are sized from PATH_MAX, not from a round
# number, so the name is reported in full instead of being truncated at the
# buffer.  A 4000-character name fits inside PATH_MAX and must come back whole;
# this fails against a 1024-byte buffer, which truncates it.
LONG4000=$(awk 'BEGIN{s="";while(length(s)<4000)s=s "yyyyyyyyyy";print substr(s,1,4000)}')
printf "include('%s').\nfof(a,axiom,p(x)).\n" "$LONG4000" > "$W/inc4000.p"
# Both checks match the whole message, not just the name: prover9's banner
# echoes argv, so grepping for the name alone passes even on a build whose
# error message is truncated or which never gets as far as printing one.
chk "T2  a 4000-char filename is reported in full, not truncated" \
    'timeout 30 "$B/tptp_to_ladr" < "$W/inc4000.p" 2>&1 \
       | grep -q "file $LONG4000 not found"'
chk "T2  prover9 likewise reports a long -f name in full" \
    'timeout 30 "$B/prover9" -f "$LONG4000" 2>&1 \
       | grep -q "read_all_input, file $LONG4000 not found"'

# T   ...and the translators must still do their job: a TPTP problem has to
# survive translation well enough for prover9 to prove it, and come back.
cat > "$W/grp.p" <<'TPTP'
fof(assoc, axiom, ! [X,Y,Z] : mult(mult(X,Y),Z) = mult(X,mult(Y,Z))).
fof(ident_l, axiom, ! [X] : mult(e,X) = X).
fof(inv_l, axiom, ! [X] : mult(X,X) = e).
fof(abelian, conjecture, ! [A,B] : mult(A,B) = mult(B,A)).
TPTP
chk "T   tptp_to_ladr output proves under prover9" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/grp.p" > "$W/grp.in" 2>/dev/null \
       && timeout 60 "$B/prover9" -f "$W/grp.in" 2>&1 | grep -q "THEOREM PROVED"'
chk "T   ladr_to_tptp round-trips it back to fof()" \
    'timeout 60 "$B/ladr_to_tptp" < "$W/grp.in" 2>/dev/null | grep -q "^fof(goals,conjecture"'

# T4: tptp_to_ladr silently dropped the negation of a quantified formula.
# TPTP allows "~" to apply to a quantified formula unparenthesised, but "~"
# was declared tighter than ":", so "~ ! [X] : p(X)" parsed as
# ":(~(!([X])), p(X))"; the converter then called listterm_reverse() on a
# non-list, got the empty list, skipped its loop and returned the body --
# asserting the opposite of the input.  1930 of the 17912 first-order
# problems in TPTP v9.3.1 contain this shape.  Checked two ways: the emitted
# text, and a proof that only succeeds if the negation survived.
printf 'fof(t,axiom, ~ ! [X] : p(X) ).\n' > "$W/neg.p"
chk "T4  negation of a quantified formula survives translation" \
    'timeout 30 "$B/tptp_to_ladr" < "$W/neg.p" 2>/dev/null | grep -qF -- "-(all X p(X))"'

printf 'fof(a1,axiom, ~ ! [X] : p(X) ).\nfof(c1,conjecture, ? [X] : ~ p(X) ).\n' \
    > "$W/neg2.p"
chk "T4  ...and the resulting problem proves correctly" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/neg2.p" > "$W/neg2.in" 2>/dev/null \
       && timeout 60 "$B/prover9" -f "$W/neg2.in" 2>&1 | grep -q "THEOREM PROVED"'

# T5: a single-quoted TPTP formula name was copied verbatim into label(...),
# but TPTP quotes with ' and LADR with ", so the translator emitted a file
# LADR's own reader rejected ("sread_term error").  382 problems affected.
printf "fof('EQ-var',axiom, p(a) ).\n" > "$W/qn.p"
chk "T5  single-quoted TPTP formula name is readable by prover9" \
    'timeout 30 "$B/tptp_to_ladr" < "$W/qn.p" > "$W/qn.in" 2>/dev/null \
       && ! timeout 30 "$B/prover9" -f "$W/qn.in" 2>&1 | grep -q "sread_term error"'

# T6: TPTP's "distinct object" token, written in double quotes.  LADR quotes
# with ' here, so a double quote was not even lexable and every problem
# containing one failed with "sread_term error" -- 239 of the 1060 Commonsense
# Reasoning problems, whose axiom files render Cyc's numerals this way.  Each
# one now becomes a constant and their pairwise inequality, which TPTP leaves
# implicit, is stated.  Checked by proving an inequality that only follows if
# the axioms are there.
printf 'fof(ax,axiom, p("Apple") & p("Microsoft") & p("Sun") ).\nfof(g,conjecture, "Apple" != "Microsoft" ).\n' \
    > "$W/do.p"
chk "T6  distinct objects translate" \
    'timeout 30 "$B/tptp_to_ladr" < "$W/do.p" > "$W/do.in" 2>/dev/null'
chk "T6  ...and their implicit inequality is provable" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/do.p" > "$W/do.in" 2>/dev/null \
       && timeout 60 "$B/prover9" -f "$W/do.in" 2>&1 | grep -q "THEOREM PROVED"'
# ...but a distinct object may still equal an ordinary term, so this must NOT
# be provable.  Without this the test above would pass on a translator that
# simply made everything unequal.
printf 'fof(ax,axiom, p("a") & p("b") & p(cat) ).\nfof(g,conjecture, "a" != cat ).\n' \
    > "$W/dc.p"
chk "T6  a distinct object may still equal an ordinary term" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/dc.p" > "$W/dc.in" 2>/dev/null \
       && ! timeout 60 "$B/prover9" -f "$W/dc.in" 2>&1 | grep -q "THEOREM PROVED"'

# T7: include() with a name list selects those formulas.  The second argument
# used to be ignored, so a selective include pulled in the whole file --
# quietly adding axioms the problem had excluded, which can make an unprovable
# conjecture provable.
mkdir -p "$W/ax"
printf 'fof(i1,axiom, p(a)).\nfof(i2,axiom, q(b)).\nfof(i3,axiom, r(c)).\n' > "$W/ax/M.ax"
printf "include('ax/M.ax',[i1,i3]).\n" > "$W/sel.p"
chk "T7  selective include/2 omits the unnamed formulas" \
    'cd "$W" && timeout 30 "$B/tptp_to_ladr" < sel.p 2>/dev/null \
       | grep -q "label(i1)" && cd "$W" \
       && ! timeout 30 "$B/tptp_to_ladr" < sel.p 2>/dev/null | grep -q "label(i2)"'
printf "include('ax/M.ax').\n" > "$W/all.p"
chk "T7  plain include/1 still takes the whole file" \
    'cd "$W" && [ "$(timeout 30 "$B/tptp_to_ladr" < all.p 2>/dev/null \
       | grep -c "label(i[123])")" = 3 ]'

# T8: the converter has always had branches for TPTP's NAND and NOR, but the
# parser was never told the operators exist, so any problem using them failed.
printf 'fof(a,axiom, p(a) ~& q(b) ).\nfof(g,conjecture, ~ (p(a) & q(b)) ).\n' > "$W/nand.p"
chk "T8  ~& (NAND) translates and means -(a & b)" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/nand.p" > "$W/nand.in" 2>/dev/null \
       && timeout 60 "$B/prover9" -f "$W/nand.in" 2>&1 | grep -q "THEOREM PROVED"'
printf 'fof(a,axiom, p(a) ~| q(b) ).\nfof(g,conjecture, ~ (p(a) | q(b)) ).\n' > "$W/nor.p"
chk "T8  ~| (NOR) translates and means -(a | b)" \
    'timeout 60 "$B/tptp_to_ladr" < "$W/nor.p" > "$W/nor.in" 2>/dev/null \
       && timeout 60 "$B/prover9" -f "$W/nor.in" 2>&1 | grep -q "THEOREM PROVED"'

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
if [ $skip -gt 0 ]; then
  echo "($skip test(s) skipped: they need a build made with -fsanitize=address;"
  echo " see 'Correction' in FIXES.md for why a plain build cannot see them.)"
fi
if [ $fail -eq 0 ]; then echo "ALL $pass REGRESSION TESTS PASSED"; else
  echo "$fail of $((pass+fail)) REGRESSION TESTS FAILED"; fi
exit $(( fail != 0 ))
