#!/usr/bin/env python3

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
"""List direct malloc/calloc/realloc calls and whether the result is NULL-tested.

Why this exists.  Three defects in a row -- M9's 56 GB cell table, M10's
(domain+1)^arity negprop tree, M11's factor of two across every large block in
the library -- were all the same thing: an allocation that bypassed palloc, so
max_megs could not see it, whose return was never checked, so a memory limit
turned into a SIGSEGV with no searching done.  The first two were found by a
crash and a machine lockout; the third was only ever visible as a plausible
looking number.  The grep that lists them all was in hand when the first was
fixed, and only the sites named by the backtrace were acted on.

So: run this rather than grepping, and judge every UNCHECKED line.

    python3 -I test.src/audit_alloc.py ladr/*.c mace4.src/*.c provers.src/*.c

It is a heuristic -- it looks for a NULL test on the assigned variable, or a
fatal_error, within the eight lines after the call -- so it over-reports
rather than under-reports.  Two things to weigh on each hit:

  1. Is the return dereferenced before anything could test it?  That is a
     crash where a diagnostic belongs.
  2. Can the SIZE be large?  A size derived from a symbol property --
     arity, domain size, vocabulary count -- can be handed symbols no human
     wrote, because clausification manufactures them.  Those are the ones
     that take a machine down, and they must also be charged to the budget
     the program reports, via charge_external_mem().
"""
import re
import sys

CALL = re.compile(r'=\s*(?:\(\s*[^)]*\)\s*)?(?:m|c|re)alloc\s*\(')
ASSIGN = re.compile(r'\s*(?:[A-Za-z_][A-Za-z0-9_]*\s+)*(\**[A-Za-z_][A-Za-z0-9_.\->\[\]]*)\s*=')
WINDOW = 8


def base_name(var):
    for sep in ('[', '.', '->'):
        var = var.split(sep)[0]
    return var.lstrip('*')


def audit(path):
    hits = []
    try:
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    except OSError as exc:
        print('%s: %s' % (path, exc), file=sys.stderr)
        return hits
    for i, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith(('*', '/*', '//')):
            continue                      # a comment mentioning malloc
        if not CALL.search(line):
            continue
        m = ASSIGN.match(line)
        lvalue = m.group(1).lstrip('*') if m else '?'
        var = base_name(m.group(1)) if m else '?'
        window = '\n'.join(lines[i + 1:i + 1 + WINDOW])
        checked = 'fatal_error' in window
        # Test both the full lvalue and its base: a check on "c->possible"
        # must not be missed just because the base name is "c".
        for name in {lvalue, var} - {'?'}:
            q = re.escape(name)
            if re.search(r'%s\s*==\s*NULL|NULL\s*==\s*%s|!\s*%s\b' % (q, q, q), window):
                checked = True
        hits.append((path, i + 1, checked, lvalue, stripped[:60]))
    return hits


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    all_hits = []
    for path in argv:
        all_hits.extend(audit(path))
    unchecked = [h for h in all_hits if not h[2]]
    for path, line, checked, var, text in all_hits:
        print('%-28s %-10s %-16s %s'
              % ('%s:%d' % (path, line), 'checked' if checked else 'UNCHECKED', var, text))
    print('\n%d allocation sites, %d with no visible NULL test'
          % (len(all_hits), len(unchecked)))
    return 1 if unchecked else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
