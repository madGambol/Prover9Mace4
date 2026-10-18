# Running tier3

tier3 is the memory-safety gate: three sweeps that each run a corpus of real
inputs through both provers under a different kind of instrumentation. It is
separate from `tier1` and `tier2` because it needs three extra builds, an
input corpus that is not in this repository, and minutes rather than seconds.

| sweep | what only it catches |
| --- | --- |
| `sanitizer_sweep.sh` | out-of-bounds access and undefined behaviour, at the points the compiler instrumented |
| `poison_sweep.sh` | stale reads through LADR's **own** avail lists, which ASan and valgrind cannot see, because `free_mem()` never returns memory to the system allocator |
| `valgrind_sweep.sh` | reads of memory that was never written — uninitialised values, with origin tracking |

The three overlap less than they look. Each script's header explains its own
case in detail; this file is only about running them.

## What you need

- **A corpus**: a list of input files, one path per line. The published one is
  `files.txt` in the corpus repository, 168 paths. Clone that beside this tree
  and nothing else is required — in particular **tier3 does not need the TPTP
  library**, because every path in `files.txt` points inside the corpus repo.
- **A compiler with AddressSanitizer and UBSan**. GCC 15.2 was used here.
  `sanitizer_sweep.sh` verifies that the build really is instrumented, two
  ways, and stops if it is not — a sanitizer sweep that silently ran an
  uninstrumented binary is worse than no sweep.
- **valgrind**, for the third sweep only. Without it that sweep stops with a
  message; the other two need no external tool.
- **Disk and memory**: each sweep builds its own tree under `$TMPDIR`
  (`/tmp` by default) and removes nothing, so budget a few hundred MB per
  sweep. Each concurrent run is allowed up to 800 MB.

## Run it

Both arguments are required. There is no default job count, because a number
chosen on the author's machine is wrong on yours.

```sh
cd test.src
make tier3 CORPUS=../../prover9-corpus/files.txt JOBS=8
```

`CORPUS` may be relative — **a relative path inside the list is resolved
against the list file's own directory**, not the shell's, so a corpus clone
works wherever it sits. Absolute paths in the list are honoured as given, and
blank lines and `#` comments are skipped.

Expect roughly 15 minutes at `JOBS=8` on a current machine, most of it in the
sanitizer and poison sweeps; valgrind is slow per run but samples the corpus
rather than walking all of it.

## Running it on a smaller machine

`JOBS` is capped, not trusted. Each sweep computes what the machine can
actually take — `min(cores, free memory ÷ 800 MB per concurrent run)` — and
lowers a larger request, saying so:

```
    JOBS=999 is more than this machine can take; using 8
    (8 core(s), memory for 14 run(s) at 800 MB each)
```

So `JOBS=2` on a two-core laptop is the right thing to pass, and it works:
the same sweeps, the same findings, more wall-clock time. A non-number is
refused outright, and omitting `JOBS` prints the ceiling this machine allows
so you know what to ask for.

Two notes for a small or shared machine:

- The per-run budget is **CPU time** (`ulimit -t`), not a wall clock. Running
  at fewer jobs than cores does not turn a slow run into a timeout, which is
  why reducing `JOBS` changes only how long the sweep takes.
- The memory cap is computed from `MemAvailable` **at the moment the sweep
  starts**. Starting one while something else large is running gets you a
  smaller cap, which is the intent.

## Running one sweep by hand

```sh
./sanitizer_sweep.sh CORPUS_LIST [BUILD_DIR] [JOBS]
./poison_sweep.sh    CORPUS_LIST [BUILD_DIR] [JOBS]
./valgrind_sweep.sh  CORPUS_LIST [SAMPLE_SIZE] [BUILD_DIR] [JOBS]
```

`JOBS` can be the positional argument or the environment variable, so
`JOBS=2 ./poison_sweep.sh list` and `./poison_sweep.sh list /tmp/b 2` are the
same. `SAMPLE_SIZE` defaults to 40 problems (80 prover/problem pairs) and
walks the corpus at a stride, so a sample is spread across it rather than
taken from the front.

Pick a `BUILD_DIR` explicitly if you want to keep a tree for inspection after
a finding; the default is `$TMPDIR/ladr-<sweep>-$$`.

## Reading the output

Each sweep ends in one line:

```
sanitizer_sweep: CLEAN
poison_sweep: CLEAN
valgrind_sweep: CLEAN
```

Anything else is a finding, printed with the program, the input and the
diagnostic above that line. A finding is not automatically a defect in this
tree — `FIXES.md` records one case where the allocator's own bookkeeping
looked like a leak — but it is always worth running down, and the build tree
is still there to re-run it under a debugger.

## If something goes wrong

| symptom | cause |
| --- | --- |
| `valgrind is not on PATH` | install it, or run the other two sweeps |
| `JOBS must be a whole number` | a typo in `JOBS=`; it takes a count, not a flag |
| `how many concurrent runs?` | `JOBS=` was omitted; the message prints the ceiling |
| every input fails to open | the corpus list's paths do not resolve — check that `CORPUS` points at the list, and that the files sit beside it |
| the instrumentation check fails | the compiler accepted `-fsanitize=…` but produced an uninstrumented object; the sweep stops rather than report a clean run |

---

Copyright (C) 2026 Chris Arena, 2026-10-18.  Written in collaboration
with Claude Opus 5 (Anthropic).  GPL-2.0-only; see `COPYING`.  Not part
of the LADR-2009-11A release.
