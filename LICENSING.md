# Licensing

Short version: this tree is **GPL-2.0-only**, the same licence upstream used,
and it cannot be anything else. [`COPYING`](COPYING) holds the licence text.

## What the upstream files say

250 of the LADR-2009-11A release's 307 `.c` and `.h` files carry the same
notice, byte-for-byte — every compiled one:

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
    on 2026-10-11 through 2026-10-16, and is not as William McCune released it
    in LADR-2009-11A. What changed and why is in FIXES.md; this file's own
    diff against the unmodified release is

        git diff 25b4505 -- LICENSING.md
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

Every file added here carries the library's own licence text with

```
Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
```

and a paragraph saying plainly that it is new in this tree and that William
McCune did not write it. That covers the 13 `.c`/`.h`/`.cpp` files (the tests
under `test.src/`, plus `ladr/cxx_alloc.h`, `ladr/i2accum_cxx.cpp` and
`ladr/symtab_cxx.cpp`), the 21 shell scripts and `test.src/audit_alloc.py`.

The eight prover input files under `test.src/demos/` carry a three-line
version of the same thing in `%` comments rather than the full text, because
they are input to a theorem prover that echoes what it reads, and a 20-line
header in the middle of a demonstration's output obscures the thing being
demonstrated. The seven Markdown files carry a one-line footer, for the same
reason in reverse: a licence block above the first heading would be the first
thing a reader sees.

All 13 source files first carried `Copyright (C) 2006, 2007 William McCune`,
because each was started from the header of the file it tests. That was a
misattribution of authorship in the one direction that matters — it credited
work to someone who did not do it — and is the reason this section exists.

**On naming an AI as a copyright holder.** The line above records who wrote the
file, which is what the rest of this tree's attribution does too (the commits
carry a `Co-Authored-By` trailer). It is not a claim about what is
copyrightable: the US Copyright Office's position is that purely
machine-generated material is not, so in practice the enforceable copyright in
these files is Chris Arena's. Recording the authorship honestly and leaving the
legal question to the law seemed better than quietly dropping half of it.

## Upstream files that shipped without the notice

Twenty files in LADR-2009-11A carry no licence text at all: the seven
Makefiles, `VERSION_DATE.h`, the five dead sources under `apps.src/old/`,
four Python scripts (`rev.py`, `apps.src/sig.py`, `ladr/kol.py`,
`ladr/util/options-for-gui.py`) and `utilities/attack`, `utilities/looper`,
`utilities/prover9-mace4`. (`utilities/gvizify` has its own, David A.
Wheeler's.)

Each now carries McCune's standard notice, and each says, in the notice
itself, that it was **added in this tree** and that he did not write those
lines there — so the text states the terms the release carries without
pretending to be a notice he wrote. Where adding it was the file's only
change, the §2(a) notice says exactly that.

Left alone deliberately: the HTML manual under `web/`, the example and
expected-output files, the manpages, and McCune's own working notes under
`TODO/` and `utilities-old/`. They are his documents and data rather than
parts of the program, and editing them to add a notice would alter the
record of what he published.

## Things to decide before publishing

These are points of fact about what the licence text asks for, not legal
advice. If it matters commercially, ask a lawyer rather than taking a
maintenance tree's README at face value.

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

---

Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.  GPL-2.0-only; see
`COPYING`.  Not part of the LADR-2009-11A release.
