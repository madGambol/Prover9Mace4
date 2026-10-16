#!/bin/bash

#  Copyright (C) 2026 Chris Arena, 2026-10-16.
#  Written in collaboration with Claude Opus 5 (Anthropic).
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written for the audit of his release
#  that FIXES.md describes, and carries the library's licence so that
#  it can be distributed with it.
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
# Mace4 soundness check: every model Mace4 reports must actually satisfy the
# clauses it was given.  interpformat renders the models, and clausetester
# independently evaluates each clause in each model, so this validates the
# model search end to end rather than just checking that it produced output.
#
#   $1 = directory holding the binaries to test
BASE=$(cd "$(dirname "$0")/.." && pwd)
BIN="${1:-$BASE/bin}"
W=$(mktemp -d "${TMPDIR:-/tmp}/ladr-soundness.XXXXXX")

mk() {  # mk <name> <clause-body-file-content on stdin>
  name=$1
  { echo 'assign(max_models, -1).'; cat; } > "$W/$name.in"
}

# --- quasigroups (infix ops, needs op() declarations) ---
cat > "$W/qg.body" <<'EOF'
op(400, infix, *). op(400, infix, \). op(400, infix, /).
formulas(theory).
x * (x \ y) = y.
x \ (x * y) = y.
(x / y) * y = x.
(x * y) / y = x.
end_of_list.
EOF

# --- groups ---
cat > "$W/grp.body" <<'EOF'
formulas(theory).
(x * y) * z = x * (y * z).
e * x = x.
x * e = x.
i(x) * x = e.
x * i(x) = e.
end_of_list.
EOF

# --- a relational problem: strict partial order with 2 incomparable elements ---
cat > "$W/ord.body" <<'EOF'
formulas(theory).
-L(x,x).
-L(x,y) | -L(y,z) | L(x,z).
-L(a,b).
-L(b,a).
a != b.
end_of_list.
EOF

# --- a problem mixing a function and a predicate ---
cat > "$W/mix.body" <<'EOF'
formulas(theory).
P(c).
-P(x) | P(f(x)).
f(f(x)) = x.
f(c) != c.
end_of_list.
EOF

for base in qg grp ord mix; do
  mk "$base" < "$W/$base.body"
  cp "$W/$base.body" "$W/$base.clauses"
done

fail=0; checked=0
for base in qg grp ord mix; do
  for n in 2 3 4; do
    "$BIN/mace4" -n "$n" -m -1 -f "$W/$base.in" 2>/dev/null \
      | "$BIN/interpformat" standard > "$W/$base.$n.interps" 2>/dev/null
    nmodels=$(grep -c "^interpretation" "$W/$base.$n.interps")
    if [ "$nmodels" -eq 0 ]; then
      printf "  %-4s n=%d : no models (unsatisfiable at this size)\n" "$base" "$n"
      continue
    fi
    out=$("$BIN/clausetester" "$W/$base.$n.interps" commands \
          < "$W/$base.clauses" 2>&1)
    verdicts=$(printf '%s\n' "$out" \
               | grep -cE "^% interp [0-9]+ models [0-9]+ of [0-9]+ clauses")
    if [ "$verdicts" -ne "$nmodels" ]; then
      printf "  %-4s n=%d : *** %d models but %d verdicts ***\n" \
        "$base" "$n" "$nmodels" "$verdicts"
      printf '%s\n' "$out" | tail -5
      fail=1; continue
    fi
    bad=$(printf '%s\n' "$out" \
          | sed -nE 's/^% interp [0-9]+ models ([0-9]+) of ([0-9]+) clauses.*/\1 \2/p' \
          | awk '$1 != $2' | wc -l)
    checked=$((checked + nmodels))
    if [ "$bad" -eq 0 ]; then
      printf "  %-4s n=%d : %5d model(s), all satisfy every clause\n" \
        "$base" "$n" "$nmodels"
    else
      printf "  %-4s n=%d : *** %d of %d MODELS DO NOT SATISFY THE CLAUSES ***\n" \
        "$base" "$n" "$bad" "$nmodels"
      printf '%s\n' "$out" \
        | sed -nE 's/^(% interp [0-9]+ models ([0-9]+) of ([0-9]+) clauses.*)/\1/p' \
        | awk -F'[ ]+' '$5 != $7' | head -5
      fail=1
    fi
  done
done

echo
if [ $fail -eq 0 ]; then
  echo "SOUNDNESS OK: $checked models verified, every one satisfies every input clause"
else
  echo "SOUNDNESS FAILURE"
fi
rm -rf "$W"
exit $fail
