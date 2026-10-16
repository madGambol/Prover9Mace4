#!/usr/bin/env python3

# CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena on
# 2026-10-11, and is not as William McCune released it in LADR-2009-11A. What
# changed and why is in FIXES.md; this file's own diff against the unmodified
# release is
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





