# Crash regression: FIXES.md's reproducers against any LADR build

This directory re-runs every reproducer in [`../FIXES.md`](../FIXES.md) that a
user can trigger from the command line or an input file. That covers each
crash, abort, hang, sanitizer error and wrong answer. The runs can target any
LADR `bin/`: this tree's, the 2009 release's, or another fork's. Build-only,
hygiene and performance-only fixes are skipped.

The first run, against LADR-2026 (AlgorithmicTruth/Prover9 at `62ae6ff`), is
written up in [`RESULTS.md`](RESULTS.md). In that build 9 of the 32
runtime-testable fixes were fixed, and 20 still crashed or misbehaved.

## Running it

```sh
./make_cases.sh                                   # writes cases/ (uses ../bin for reference proofs)
./run_all.sh ../bin old-fixed                     # this tree's build
./run_all.sh /path/to/other/bin new-release       # any other LADR bin/
./run_all.sh /path/to/other/bin new-release M8 T4a   # only some cases
python3 tabulate.py logs                          # side-by-side verdicts
```

- `run_all.sh` works out the build type from the binaries:
  - a plain build runs the ordinary cases;
  - an ASan/UBSan build runs the sanitizer cases;
  - a poisoned build runs M8.
- Results go to `logs/<label>/summary.tsv`, with each case's `.cmd`, `.out`
  and `.err` alongside.
- Every run is wrapped in `timeout`, and plain builds get a 16 GB `ulimit -v`.
  The cases run one at a time.
- The M11 case needs the TPTP library. Set `TPTP_DIR`; the default is
  `~/Projects/TPTP/TPTP-v9.3.1`.

The 2009 baseline and the instrumented builds come from
`NEW_TREE=/path/to/LADR-2026 build/build.sh [all|base|new-asan|...]`. It builds
under `build/` at `-j8` and only reads this tree (`git archive`) and
`NEW_TREE`. M8 is found with `wrap/poison_wrap.c`, a link-time stand-in for
`-DPOISON_FREED_MEM` that needs no edits to the tree under test.

## What is tracked

Only the scripts, `wrap/poison_wrap.c` and the two hand-written unit
reproducers in `cases/unit/` are tracked. `cases/`, `logs/` and `build/` are
regenerated.

---

Copyright (C) 2026 Chris Arena, 2026-10-27.  Written in collaboration
with Claude Opus 5.5 (Anthropic).  GPL-2.0-only; see `../COPYING`.  Not part
of the LADR-2009-11A release.
