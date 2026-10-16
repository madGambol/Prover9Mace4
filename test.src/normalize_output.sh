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
# Strip the run-dependent parts of prover9/mace4 output, so that two runs can
# be compared for a difference in what they CONCLUDED rather than in how long
# they took or where they were installed.
#
#   usage:  prover9 -f problem | test.src/normalize_output.sh
#
# This exists as one script, in the repository, because writing it inline is
# how it kept going wrong.  Five slightly different copies of this filter were
# used during the work in FIXES.md, and two of them were defective in ways
# that produced false findings:
#
#   - one omitted the banner line, which echoes the binary's own path.  An A/B
#     between two builds in different directories then reported that 336 of
#     336 runs differed.
#
#   - one filtered "wall" case-sensitively and so missed "Wall_clock=", the
#     elapsed seconds.  A run taking 0 seconds and one taking 1 second then
#     looked like a behavioural difference.  That cost a long detour chasing
#     nondeterminism in prover9 that turned out to be this line; valgrind with
#     --track-origins confirmed afterwards that the run has no uninitialised
#     reads at all.
#
# Everything removed here is either a timing, a memory figure, a process id,
# or a path.  Nothing that reflects a conclusion -- proofs, models, clause
# counts, exit reasons -- is touched.

grep -vaiE \
  -e 'The command was' \
  -e 'Wall_clock|User_CPU|System_CPU' \
  -e '[0-9]{2}:[0-9]{2}:[0-9]{2}' \
  -e '^(Mon|Tue|Wed|Thu|Fri|Sat|Sun) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)' \
  -e 'seconds|secs|[^a-z]hz' \
  -e 'megs|Kbytes|bytes|memory|malloc' \
  -e 'process [0-9]|^Process |pid ' \
  -e '^Total|sb buffer|^chunk|^string_buf|^term \(|^symbol \(' \
  -e 'Memory report|^-+$' \
| sed -E '
    s/[0-9]+\.[0-9]+//g;      # any decimal: times, rates
    s/\b[0-9]{5,}\b//g;       # long integers: addresses, large counters
  '
