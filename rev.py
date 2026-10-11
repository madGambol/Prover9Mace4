#!/usr/bin/env python3

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





