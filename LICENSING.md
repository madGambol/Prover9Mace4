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
`test.src/` — is offered under the same terms, to keep the tree uniform.

## Things to decide before publishing

These are points of fact about what the licence text asks for, not legal
advice. If it matters commercially, ask a lawyer rather than taking a
maintenance tree's README at face value.

- **GPL-2 §2(a)** asks that modified files "carry prominent notices stating
  that you changed the files and the date of any change." Right now the record
  of changes lives in `FIXES.md` and the git history, which is clear and
  auditable but is not a notice *in each modified file*. If you want to follow
  §2(a) to the letter, the usual approach is a line under the existing notice
  in each file that was touched, for example:

  ```
      Modified 2026 by <name>: see FIXES.md.  Changes are GPL-2.0-only,
      as above.
  ```

  34 `.c`/`.h` files were modified (45 files in total, counting Makefiles and
  scripts), so this is a mechanical change; say the word and I can add it
  without touching anything else.

- **Three helper scripts carry no licence header at all** upstream:
  `utilities/attack`, `utilities/looper` and `utilities/prover9-mace4`.
  (`utilities/gvizify` does.) They are plainly part of the same package and
  distributed under the same terms, but if you want the tree to be
  self-describing, adding the standard header to those three would remove the
  ambiguity.

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
