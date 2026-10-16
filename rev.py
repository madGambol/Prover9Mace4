#!/usr/bin/env python3

#  Copyright (C) 2006, 2007 William McCune
#
#  NOTICE ADDED IN THIS TREE: the LADR-2009-11A release shipped this
#  file without a licence notice.  What follows is the notice every
#  .c and .h file of that release carries, reproduced so that this
#  file is self-describing.  The terms are the release's own; this
#  is not a new grant, and McCune did not write these lines here.
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


# CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena on
# 2026-10-11 through 2026-10-16, and is not as William McCune released it in
# LADR-2009-11A. What changed and why is in FIXES.md; this file's own diff
# against the unmodified release is
#
#     git diff 25b4505 -- rev.py

import sys
import re

lines = sys.stdin.readlines()

entries = []
entry = []

for line in lines:
	if re.match(r'\S', line) or re.search('------', line):
		entries.append(entry)
		entry = []
	entry.append(line)

entries.append(entry)

entries.reverse()

for entry in entries:
	for line in entry:
		sys.stdout.write(line)





