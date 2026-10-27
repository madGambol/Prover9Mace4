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

# tabulate.py LOGDIR -- side-by-side verdicts from logs/*/summary.tsv
import sys, csv, os
L = sys.argv[1]
plain = ['baseline-2009', 'old-fixed', 'new-release']
inst = ['baseline-2009-asan', 'old-fixed-asan', 'new-asan',
        'baseline-2009-poison', 'old-fixed-poison', 'new-poison']
d, order = {}, []
for lab in plain + inst:
    f = os.path.join(L, lab, 'summary.tsv')
    if not os.path.exists(f): continue
    for r in csv.DictReader(open(f), delimiter='\t'):
        if r['case'] not in order: order.append(r['case'])
        d[(lab, r['case'])] = r
def cell(lab, c, w=30):
    r = d.get((lab, c))
    return ('' if not r else f"{r['verdict']} {r['class']}")[:w]
def table(labs):
    print(f"{'case':6} | " + " | ".join(f"{l:30}" for l in labs))
    for c in order:
        if any((l, c) in d for l in labs):
            print(f"{c:6} | " + " | ".join(f"{cell(l, c):30}" for l in labs))
    print()
table(plain); table(inst[:3]); table(inst[3:])
for lab in ['new-release', 'new-asan', 'new-poison']:
    print(f"--- {lab} notes")
    for c in order:
        r = d.get((lab, c))
        if r and r['verdict'] not in ('pass', 'n/a'):
            print(f"  {c:6} {r['verdict']:9} rc={r['rc']:4} {r['secs']:>7}s {r['note'][:150]}")
