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
# A measurement, not a pass/fail test: how long tptp_to_ladr takes to rename n
# symbols that are not legal LADR symbols.
#
# THE DEFECT
#   fresh_symbol(prefix, arity) builds a name from the prefix and "the
#   smallest natural number that results in a new symbol", and it looked for
#   that number starting at 0 on EVERY call.  str_exists(), which it consults,
#   scans the entire symbol table.  Producing n fresh symbols with one prefix
#   therefore cost O(n^2) full table scans.
#
# WHY IT MATTERS FOR A LARGE KNOWLEDGE BASE
#   tptp_to_ladr renames every symbol that is illegal in LADR, and TPTP axiom
#   files have thousands: Axioms/BIO001+0.ax has 25,062 and
#   Axioms/CSR003+2.ax has 14,265.  Translating the larger Commonsense
#   Reasoning problems spent nearly all of its time here, and the largest
#   never finished.  This is the shape of problem a Cyc-sized knowledge base
#   hits constantly: an operation that is fine at 100 symbols and fatal at
#   10,000.
#
# MEASURED, before and after
#
#       n        2009 code     this tree
#       200       0.16 s        0.024 s
#       400       1.11 s        0.033 s
#       800       7.92 s        0.062 s
#      1600      62.96 s        0.143 s      440x
#      3200      (hours)        0.533 s
#
#   The fix remembers the high-water mark per prefix, leaving one table scan
#   per call instead of n of them.  The contract weakens from "the smallest
#   natural number" to "a natural number", which costs nothing: the names are
#   arbitrary and every candidate is still checked against the symbol table.
#
# RUN IT
#   test.src/demos/perf-fresh-symbol.sh [bin-directory]
#------------------------------------------------------------------------------
B="${1:-$(cd "$(dirname "$0")/../.." && pwd)/bin}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
echo "renaming n illegal symbols with $B/tptp_to_ladr:"
for n in 200 400 800 1600 3200; do
  python3 -I - "$n" > "$W/q.p" <<'PY'
import sys
n = int(sys.argv[1])
print("fof(a,axiom,")
print(" & ".join(f"'q*{i}'(a)" for i in range(n)))     # q*i is illegal in LADR
print(").")
PY
  s=$(date +%s%3N)
  ( ulimit -t 300; "$B/tptp_to_ladr" < "$W/q.p" > "$W/out" 2>/dev/null )
  e=$(( $(date +%s%3N) - s ))
  u=$(grep -oE 'ladr[0-9]+' "$W/out" 2>/dev/null | sort -u | wc -l)
  printf '  n=%-5s %7s ms   distinct replacements: %s\n' "$n" "$e" "$u"
done
echo "(distinct replacements must equal n: the speed-up must not reuse a name)"
