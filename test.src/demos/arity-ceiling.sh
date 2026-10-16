#!/bin/bash

#  Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
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
#------------------------------------------------------------------------------
# The arity ceiling: MAX_ARITY was 255, and real input needs more.
#
# THE DEFECT, IN TWO PARTS
#
#   Too small.  ladr/term.h defined
#
#       #define MAX_ARITY UCHAR_MAX   /* max arity of any term
#                                        (don't make this big) */
#
#   so no term could take more than 255 arguments.  That is not enough for
#   input people actually have: clausifying a first-order problem introduces
#   Skolem functions whose arity is the number of enclosing universal
#   variables, and TPTP v9.3.1 contains problems where that reaches 350.
#   Seven first-order problems could not be read at all, failing with
#
#       Arity 280 requested for symbol "esk1426_280"
#       Fatal error:  get_rigid_term, arity too big
#
#   Inconsistently typed.  The arity lived in an "unsigned char" field in
#   struct term and struct flatterm, but sixty-odd declarations passed it
#   around as "int", and nothing between the two checked the range.  Only
#   get_rigid_term() checked MAX_ARITY; get_term(), which is where a term is
#   actually allocated and where the field is assigned, did not.  So any path
#   that reached get_term() with an arity above the field's width truncated it
#   silently -- 280 became 280 - 256 = 24 -- and the term then disagreed with
#   its own args array about how many arguments it had.  That is a wrong
#   answer rather than an error, which is the worse of the two failures.
#
# THE FIX
#   A named type, ArityType, used everywhere the quantity is an arity instead
#   of whichever integer type was at hand.  It is unsigned short, so
#   MAX_ARITY is 65534, and ARITY_NONE (65535) names the "not an arity" case
#   that used to be spelled -1 (a failed symbol lookup, or a symbol whose
#   arity is unspecified).  The range check moved into get_term(), the single
#   allocation point, so no path can truncate.
#
# IT COSTS NO MEMORY
#   struct term is the most numerous object in the system.  The wider field
#   fits in padding that was already there:
#
#       int        private_symbol;   4 bytes
#       ArityType  arity;            2
#       FLAGS_TYPE private_flags;    1
#                                    1 byte of padding
#       Term       *args;            8, and 8-aligned
#
#   sizeof(struct term) is 32 before and after.  ../arity_test.c asserts that,
#   so a later change that quietly makes every term bigger has to argue for
#   itself.
#
# MEASURED
#   Of the seven problems that failed with "arity too big", five now
#   translate: KRS287-1, KRS288-1, KRS289-1, KRS290-1, KRS291-1.  The other
#   two, SYO599-1 and SYO601-1, have arities of 317 and 350 which are now
#   within the ceiling -- they fail for a different and separate reason, the
#   translator's remaining super-linear behaviour on very large inputs, and
#   are killed by a CPU limit rather than refused.
#
# RUN IT
#   test.src/demos/arity-ceiling.sh [bin-directory]
#
#   Needs the TPTP library at ~/Projects/TPTP for the real problems; without
#   it, only the synthetic check runs.
#------------------------------------------------------------------------------
B="${1:-$(cd "$(dirname "$0")/../.." && pwd)/bin}"
T="$HOME/Projects/TPTP/TPTP-v9.3.1"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

echo "synthetic: a predicate of arity 300, which the old ceiling refused"
# Constants, not variables.  An earlier version of this used 300 distinct
# universally quantified variables and hit MAX_VARS (100 distinct variables
# per term) instead -- a different limit entirely, which made the
# demonstration prove nothing about arity.  Constants isolate the arity.
python3 -I - > "$W/a300.p" <<'PY'
n = 300
args = ",".join(f"c{i}" for i in range(n))
print(f"fof(ax, axiom, p({args}) ).")
print(f"fof(goal, conjecture, p({args}) ).")
PY
if "$B/tptp_to_ladr" < "$W/a300.p" > "$W/a300.in" 2>"$W/err"; then
  printf '  translated.  prover9 says: %s\n' \
    "$("$B/prover9" -f "$W/a300.in" 2>&1 | grep -E 'THEOREM PROVED|SEARCH FAILED' | head -1)"
else
  printf '  REFUSED: %s\n' "$(grep -m1 -i arity "$W/err")"
fi

if [ -d "$T" ]; then
  echo "the real problems, from TPTP v9.3.1:"
  for p in KRS287-1 KRS288-1 KRS289-1 KRS290-1 KRS291-1 SYO599-1 SYO601-1; do
    f=$(find "$T/Problems" -name "$p.p" 2>/dev/null | head -1)
    [ -n "$f" ] || continue
    a=$(grep -ohE 'esk[0-9]+_[0-9]+' "$f" | sed 's/.*_//' | sort -n | tail -1)
    if ( cd "$T" && ulimit -s 1000000; ulimit -t 120; "$B/tptp_to_ladr" ) < "$f" >/dev/null 2>"$W/e"
    then printf '  %-10s max arity %-4s translated\n' "$p" "$a"
    else
      # A CPU-limit kill leaves no message, so say which it was rather than
      # printing an empty parenthesis.
      if [ -s "$W/e" ]; then reason=$(head -1 "$W/e" | cut -c1-44)
      else reason="killed by the 120s CPU limit, not refused"; fi
      printf '  %-10s max arity %-4s not translated (%s)\n' "$p" "$a" "$reason"
    fi
  done
else
  echo "(TPTP not found at $T, skipping the real problems)"
fi
