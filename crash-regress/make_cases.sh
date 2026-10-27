#!/bin/bash

#
#  Copyright (C) 2026 Chris Arena, 2026-10-27.
#  Written in collaboration with Claude Opus 5.5 (Anthropic).
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written to re-run the reproducers that
#  FIXES.md describes against other LADR builds, and carries the
#  library's licence so that it can be distributed with it.
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
#

# Generates every input in cases/ from FIXES.md / test.src/regress.sh /
# test.src/mace4_cells_test.sh / test.src/demos of this tree.
# Reference outputs (proof.txt, p.xml, alg033.in) are produced ONCE with the
# old fixed build so that every build under test reads identical bytes.
set -eu
S=$(cd "$(dirname "$0")" && pwd)
C=$S/cases
OLD=$(cd "$S/.." && pwd)          # this tree: crash-regress/ sits next to FIXES.md
REF=$OLD/bin                       # old fixed build, used only to make inputs
TPTP="${TPTP_DIR:-$HOME/Projects/TPTP/TPTP-v9.3.1}"   # only M11 needs it
mkdir -p "$C/ax" "$C/unit"

printf 'formulas(assumptions).\nP(a).\nend_of_list.\n'   > "$C/triv.in"
printf 'formulas(assumptions).\nx = x.\nend_of_list.\n'  > "$C/tiny.in"
printf 'f(%%s%%s%%s%%s%%s%%n,a)\n'                       > "$C/fmt.in"
cp "$OLD/prover9.examples/x2.in" "$C/x2.in"

# a complete proof, and one truncated right after its "% Proof" comment line
"$REF/prover9" -f "$C/x2.in" < /dev/null > "$C/proof.txt" 2>&1 || true
awk '/= PROOF =/{p=1} {print} p&&/^% Proof/{exit}' "$C/proof.txt" > "$C/trunc.txt"
# D2: a proof whose first clause id is a 200,000-character token
awk 'BEGIN{s="";while(length(s)<200000)s=s "xxxxxxxxxx";print substr(s,1,200000)}' > "$C/tok.txt"
awk -v tokfile="$C/tok.txt" 'BEGIN{getline tok < tokfile}
  /= PROOF =/{p=1}  p && !done && /^[0-9]+ /{sub(/^[0-9]+/, tok); done=1}  {print}' \
  "$C/proof.txt" > "$C/longtok.txt"
rm -f "$C/tok.txt"
"$REF/prooftrans" xml < "$C/proof.txt" > "$C/p.xml" 2>/dev/null || true

# M7: production mode (regress.sh's prod.in, plus the demo file)
printf 'set(production).\nassign(max_seconds,5).\nformulas(sos).\n  p(1 + 1).\n  q(max(3,5)).\nend_of_list.\n' > "$C/prod.in"
cp "$OLD/test.src/demos/m7-production-overflow.in" "$C/m7-production-overflow.in"

# T1/M3 and T2: include() of a missing file with a long name
LONG=$(printf 'z%.0s' $(seq 1 300))
LONG4000=$(awk 'BEGIN{s="";while(length(s)<4000)s=s "yyyyyyyyyy";print substr(s,1,4000)}')
printf "include('%s').\nfof(a,axiom,p(x)).\n" "$LONG"     > "$C/longinc.p"
printf "include('%s').\nfof(a,axiom,p(x)).\n" "$LONG4000" > "$C/inc4000.p"
# T3: a circular include (the file includes itself)
printf "include('loop.p').\nfof(a,axiom,p(x)).\n" > "$C/loop.p"

# translator round trip, T4, T5, T6, T7, T8, arity ceiling
cat > "$C/grp.p" <<'TPTP'
fof(assoc, axiom, ! [X,Y,Z] : mult(mult(X,Y),Z) = mult(X,mult(Y,Z))).
fof(ident_l, axiom, ! [X] : mult(e,X) = X).
fof(inv_l, axiom, ! [X] : mult(X,X) = e).
fof(abelian, conjecture, ! [A,B] : mult(A,B) = mult(B,A)).
TPTP
printf 'fof(t,axiom, ~ ! [X] : p(X) ).\n' > "$C/neg.p"
printf 'fof(a1,axiom, ~ ! [X] : p(X) ).\nfof(c1,conjecture, ? [X] : ~ p(X) ).\n' > "$C/neg2.p"
# the nested form from FIXES.md's T4 table: provable only if the inner negation survives
printf 'fof(a1,axiom, ! [X] : ~ ? [Y] : q(X,Y) ).\nfof(c1,conjecture, ~ q(a,b) ).\n' > "$C/negnest.p"
# the inverted-axiom form: with the bug the axiom "~ ! [X] : p(X)" becomes p(X),
# i.e. all X p(X), so p(c) is WRONGLY provable; correctly translated it is not.
printf 'fof(a1,axiom, ~ ! [X] : p(X) ).\nfof(c1,conjecture, p(c) ).\n' > "$C/neginv.p"
printf "fof('EQ-var',axiom, p(a) ).\n" > "$C/qn.p"
printf 'fof(ax,axiom, p("Apple") & p("Microsoft") & p("Sun") ).\nfof(g,conjecture, "Apple" != "Microsoft" ).\n' > "$C/do.p"
printf 'fof(ax,axiom, p("a") & p("b") & p(cat) ).\nfof(g,conjecture, "a" != cat ).\n' > "$C/dc.p"
printf 'fof(i1,axiom, p(a)).\nfof(i2,axiom, q(b)).\nfof(i3,axiom, r(c)).\n' > "$C/ax/M.ax"
printf "include('ax/M.ax',[i1,i3]).\n" > "$C/sel.p"
printf "include('ax/M.ax').\n" > "$C/all.p"
# T7 semantic form: q(b) is only provable if the excluded i2 leaked in
printf "include('ax/M.ax',[i1,i3]).\nfof(g,conjecture, q(b) ).\n" > "$C/selq.p"
printf 'fof(a,axiom, p(a) ~& q(b) ).\nfof(g,conjecture, ~ (p(a) & q(b)) ).\n' > "$C/nand.p"
printf 'fof(a,axiom, p(a) ~| q(b) ).\nfof(g,conjecture, ~ (p(a) | q(b)) ).\n' > "$C/nor.p"
python3 -I - > "$C/a300.p" <<'PY'
n = 300
args = ",".join(f"c{i}" for i in range(n))
print(f"fof(ax, axiom, p({args}) ).")
print(f"fof(goal, conjecture, p({args}) ).")
PY

# M9/M10/M12: Mace4 tables (test.src/mace4_cells_test.sh)
args() { i=2; printf 'x1'; while [ $i -le "$1" ]; do printf ',x%d' $i; i=$((i+1)); done; }
mk() { { echo 'assign(max_megs,500).'; echo 'formulas(sos).'; printf '%s(%s).\n' "$2" "$(args "$3")"; echo 'end_of_list.'; } > "$1"; }
mk "$C/wide.in" p 40
mk "$C/big.in"  q 29
{ echo 'assign(max_megs,20).'; echo 'formulas(sos).'; printf 'r(%s).\n' "$(args 15)"; echo 'end_of_list.'; } > "$C/np.in"
{ echo 'assign(max_megs,20).'; echo 'assign(max_seconds,60).'; echo 'formulas(sos).'
  echo 'p(x1,x2) | p(x3,x4) | p(x5,x6) | p(x7,x8) | q(x1,x3,x5,x7).'
  echo '-p(x1,x2) | -q(x1,x2,x3,x4) | r(x3,x4).'; echo 'end_of_list.'; } > "$C/ground.in"
printf 'formulas(sos).\na(x) | b(x).\nend_of_list.\n' > "$C/ok.in"

# N1-N3: derived parms
for spec in n1_min:'assign(max_minutes,-1).' n1_hrs:'assign(max_hours,-1).' \
            n1_days:'assign(max_days,-1).' n2_min:'assign(max_minutes,50000000).' \
            n2_hrs:'assign(max_hours,1000000).' n3_pgr:'assign(pick_given_ratio,-1).' \
            nc_min:'assign(max_minutes,2).' nc_hrs:'assign(max_hours,3).' nc_pgr:'assign(pick_given_ratio,4).'; do
  printf '%s\n' "${spec#*:}" > "$C/${spec%%:*}.opt.in"
done

# M11: ALG033+1 (FIXES.md: holds ~1,068 MB while reporting a 500 MB ceiling)
if [ -f "$TPTP/Problems/ALG/ALG033+1.p" ]; then
  ( cd "$TPTP" && "$REF/tptp_to_ladr" < Problems/ALG/ALG033+1.p ) > "$C/alg033.in" 2>/dev/null || true
  printf 'assign(max_megs,500).\n' > "$C/megs500.in"
fi

# X1-X5 helper scripts
printf 'formulas(assumptions).\n(x * y) * z = x * (y * z).\nend_of_list.\n' > "$C/head.in"
printf 'x * y = y * x.\n%% comment\nx * x = x.\n' > "$C/cands.txt"

# U2/U3 probes (FIXES.md gives no reproducer; these did not trigger either
# bug even on the 2009 UBSan build, so run_all.sh runs them only on request)
for u in u2:4 u3:0; do
  cat > "$C/${u%%:*}_probe.in" <<EOF
assign(false_part, 2147483640).
assign(true_part, ${u#*:}).
assign(age_part, 1).
assign(weight_part, 0).
assign(sos_limit, 20).
assign(max_given, 150).
assign(max_seconds, 30).
clear(print_given).
formulas(sos).
  e * x = x.
  x' * x = e.
  (x * y) * z = x * (y * z).
  x * x = e.
end_of_list.
list(interpretations).
interpretation(2, [number=1, seconds=0], [
  function(e, [0]),
  function('(_), [0,1]),
  function(*(_,_), [0,0,1,1])]).
end_of_list.
EOF
done

# unit tests: copied verbatim from the old project (not edited)
cp "$OLD/test.src/discrim_test.c" "$OLD/test.src/intpower_test.c" "$C/unit/"
echo "cases written to $C: $(ls "$C" | wc -l) entries"
