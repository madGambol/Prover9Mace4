#!/bin/bash
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
