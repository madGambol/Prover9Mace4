# Licensing

Short version: this tree is **GPL-2.0-only**, the same licence upstream used,
and it cannot be anything else. [`COPYING`](COPYING) holds the licence text.

## What the upstream files say

All 250 `.c` and `.h` files in the LADR-2009-11A release carry the same
notice, byte-for-byte:

```
/*  Copyright (C) 2006, 2007 William McCune

    This file is part of the LADR Deduction Library.

    The LADR Deduction Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License,
    version 2.
    ...
*/
```

Two details matter:

1. **It is version 2 only.** The notice says "version 2", not "version 2 or
   any later version". There is no upgrade clause, so this code cannot be
   redistributed under GPL-3 and cannot be combined with GPL-3-only code. The
   SPDX identifier is `GPL-2.0-only`, not `GPL-2.0-or-later`.
2. **The copyright holder is William McCune**, not the person publishing this
   tree. Nothing here changes that, and nothing here may remove or alter those
   notices.

`COPYING` is upstream's own copy of the GPL-2 text and is unmodified.

## How that applies to the changes in this repository

Prover9/Mace4 is a derivative work, so the modifications inherit the licence:
when this tree is distributed, it is distributed under GPL-2.0-only, with the
original copyright notices intact. There is no option to relicense, dual-license,
or add restrictions, because the only grant received is the GPL-2 grant.

Everything added here — `FIXES.md`, `README.md`, this file, the tests under
`test.src/`, the scripts under `test.src/demos/` — is offered under the same
terms, to keep the tree uniform.

## The §2(a) change notices

GPL-2 §2(a) asks that modified files "carry prominent notices stating that you
changed the files and the date of any change." **This tree carries them**, one
per file, directly under the upstream notice:

```
/*  CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena
    on 2026-10-13 through 2026-10-16, and is not as William McCune released it
    in LADR-2009-11A. What changed and why is in FIXES.md; this file's own
    diff against the unmodified release is

        git diff 25b4505 -- ladr/discrim.c
*/
```

**91 files** carry one: 80 `.c`/`.h`, six Makefiles, four scripts under
`utilities/` and `rev.py`. The dates are each file's own — the first and last
commit that touched it — taken from the git history, not a blanket range. The
notice gives the command that produces the file's exact diff against the
pristine first commit, so a reader does not have to trust the prose.

Three details worth knowing:

- **`utilities/gvizify` is David A. Wheeler's**, not McCune's, so its notice
  names him instead.
- **`.gitignore` has no notice.** It is not McCune's file: the pristine commit
  added it alongside the unpacked tarball, so there is nothing of his to have
  changed.
- **Upstream's own notices are untouched.** The change notice is added below
  them; no copyright line, licence paragraph or warranty disclaimer of
  McCune's was altered, reflowed or removed in any file.

## Files this tree added

The 13 `.c`/`.h`/`.cpp` files added here — the tests under `test.src/`, and
`ladr/cxx_alloc.h`, `ladr/i2accum_cxx.cpp`, `ladr/symtab_cxx.cpp` — originally
carried McCune's copyright line, because each was started from the header of
the file it tests. That was a misattribution of authorship, in the one
direction that matters: it credited work to someone who did not do it. They now
read `Copyright (C) 2026 Chris Arena` and say plainly that McCune did not write
them, while keeping the library's GPL-2 grant so they can be distributed with
it.

The added shell, Python and Markdown files carry no header. They are covered by
the statement above and by this file; nothing in GPL-2 asks for a per-file
notice on new work.

## Things to decide before publishing

These are points of fact about what the licence text asks for, not legal
advice. If it matters commercially, ask a lawyer rather than taking a
maintenance tree's README at face value.

- **Three helper scripts carry no licence header at all** upstream:
  `utilities/attack`, `utilities/looper` and `utilities/prover9-mace4`.
  (`utilities/gvizify` does.) They are plainly part of the same package and
  distributed under the same terms. The standard header was deliberately *not*
  added to them, because writing a `Copyright (C) 2006, 2007 William McCune`
  line into a file he left unmarked means asserting a copyright notice on his
  behalf — a different act from recording a change of one's own. They carry
  the §2(a) change notice and nothing more.

- **No warranty.** GPL-2 §§11–12 disclaim all warranty, and that disclaimer
  comes along with the code. A maintenance tree that fixes memory-safety bugs
  should be especially careful not to imply any guarantee of correctness. The
  evidence in `FIXES.md` says what was tested and what was not; it does not say
  the code is free of defects. D2 in that document is a defect found *after* an
  earlier sweep reported zero findings, which is the honest illustration of the
  difference.

- **Attribution.** The programs' own banners and the HTML manual name McCune
  as the author. Those should stay as they are; this tree should be described
  as a repaired distribution of his release, not as a new program.
