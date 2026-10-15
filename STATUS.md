# Where this stands

As of 2026-10-14. `master` and `rc` are both at the same commit, the tree is
clean, and every gate is green. `FIXES.md` is the defect catalogue and the
place to look for detail; this file is for picking the work back up.

## The state of the tree

| | |
| --- | --- |
| branches | `master`, `rc` — in sync, no feature branches outstanding |
| upstream baseline | commit `25b4505` is the pristine 2009-11A tarball, so `git diff 25b4505` is every change |
| build | `make all`; one caveat, below, about the C++ translation units |
| gates | `make tier1`, `make tier2 BASELINE=…`, `make tier3 CORPUS=…`, plus the A/B |

Startup on `Problems/CSR/CSR040-10.p` — 44,227 clauses and 7,161 distinct
symbols after translation — went from **157.53 s to 3.80 s**, a factor of 41,
across the PF2–PF10 series. The cumulative table in `FIXES.md` attributes it
step by step.

## The build now has two C++ translation units

`ladr/i2accum_cxx.cpp` and `ladr/symtab_cxx.cpp` implement `I2accum` and
`lookup_by_id` on `std::unordered_map` and `std::vector`, behind `extern "C"`
interfaces that match the old prototypes exactly. Consequences worth knowing
before touching the build:

- `libladr.a` contains C++ objects, so the C++ runtime must be linked wherever
  it is. That is `LADRLIBS = -lstdc++`, defined in each program directory's
  Makefile and appended to the link lines. `CC` is unchanged.
- containers allocate through `LadrAlloc<T>` (`ladr/cxx_alloc.h`), which charges
  `charge_external_mem()` so their bytes still count against `max_megs`.
- every `extern "C"` body catches everything and routes to `fatal_error`; an
  exception crossing into C is undefined behaviour.
- `ladr/Makefile` has a `%.o: %.cpp` rule and `CXXFLAGS = $(XFLAGS) …`, so
  `make XFLAGS=…` still reaches both languages. The tier3 sweeps depend on
  that.

## How to gate a change

In this order. Each catches things the others cannot.

```sh
cd test.src
make tier1                                                  # ~2 s
make tier2 BASELINE=/path/to/pristine-plus-build-fixes/bin   # ~3 min
make tier3 CORPUS=~/Projects/prover9-corpus/files.txt        # ~15 min
# then the A/B, below
```

`tier2`'s `BASELINE=` is not optional in spirit: it asserts that at least 30 of
the 41 defect tests still **fail** against the unfixed code. A test that passes
on the unfixed code proves nothing, and that is the single mistake this suite
has made most often.

**The A/B is the gate that matters most and is the easiest to skip.** Build the
previous commit, run every pair in `~/Projects/prover9-corpus/comparable.txt`
under both binaries, pipe each through `test.src/normalize_output.sh`, and
compare. Run **A/A first** — the same binary against itself — because an A/B
number is meaningless without the noise floor.

Expect A/A = 0 and A/B = 2. The two are `ALG033+1` and `ALG043+1`, the only
corpus problems that terminate *by* exhausting `max_megs`, so any change in
allocation moves them; both must still reach `exit (max_megs)`.

This is not belt-and-braces. Tier2 passed green on a build that had silently
**lost three proofs** (`SWC258+1`, `SET667+3`, `olsax2`); the A/B is what found
it. Tier2 is 41 problems of a few dozen clauses and structurally cannot see a
change that only matters at scale or to an invariant.

## What is outstanding

Roughly in order of expected value.

1. **PF6 — `logic_term()` compares connective names.** It asks
   `is_term(t, and_sym(), 2)` and so on, resolving a symbol and comparing
   strings, where the connective symnums are fixed once the symbol table is
   built. 32 million calls produced 480 million `lookup_by_id` and 161 million
   `str_ident` calls in one profile. Cache the symnums and compare integers.
2. **The FPA half of PF10.** `fpa_trie_member_insert` in `ladr/fpa.c` has the
   identical defect the discrimination tree had — its own comment says
   "Children are in increasing order" above a linear scan — and was 22.87% of
   the pre-fix profile. The fix is the same shape, and so is the trap: dropping
   the ordering means auditing **every** reader of the structure, which is
   exactly what cost three proofs in the discrimination tree.
3. **An unidentified quadratic.** A ladder of unit clauses still grows about
   4.5x per doubling of the vocabulary above 60,000 symbols, with
   `predicate_elim` cleared so PF3's remaining round-per-symbol loop is out of
   the picture. Nothing in the current profile explains it; it needs
   instrumenting rather than guessing.
4. ~~The TPTP soundness oracle has not been run.~~ **Done.** Every 3rd
   problem of TPTP v9.3.1, both provers, 10 s CPU: **17,992 runs, 0
   violations**, with the detector calibrated against a deliberately
   mislabelled problem first so that a clean result means something. Capability
   alongside it: prover9 38.5% and mace4 5.9% of the 5,681 first-order problems
   with a usable status, 41.4% between them. Detail in `FIXES.md`, recipe in
   `~/Projects/prover9-corpus/README.md`.

   A follow-up re-ran Prover9's 3,361 timed-out runs at 300 s — only those,
   since an answered problem cannot change its answer by being given more
   time. **0 violations there too**, and 508 further answers: prover9
   **38.5% → 47.5%**, combined **41.4% → 50.1%**. Mace4's 5,171 re-runs were
   deliberately skipped (its timeouts are overwhelmingly theorems with no model
   to find, so ~5 h of machine time for almost no information), which means its
   5.9% is still a 10 s figure and is **not comparable** to Prover9's.

   Raw results in `~/Projects/prover9-corpus/results/`; merge and score with
   `oracle_scoreboard.sh`. The conversion rate decayed 21.4% → 15.1% as the
   run went on, so 300 s is near the point of diminishing returns — a further
   10x would add much less. Mace4 at 300 s is the obvious remaining gap if a
   like-for-like comparison is ever wanted.
5. **Two more hand-rolled containers**, if the C++ pattern is worth continuing:
   the discrimination-tree child index and the grouping in `collect_multiples`.
   Expect ~0% speed and roughly 230 fewer lines; the case is bug surface, not
   performance.
6. **`max_megs` default.** `DEFAULT_MAX_MEGS` in `ladr/memory.c` is 1000, but
   Prover9 and Mace4 override it from their own parameter, still 500
   (`provers.src/search.c`, `mace4.src/msearch.c`). Raising those changes
   results on memory-bounded runs, so it is a decision rather than a fix.
7. **`MAX_VARS`** is still 100, blocked by the `Context` arrays, the variable-ID
   stride multiplier, `compress.c`'s char assumption and `MAX_BASIS`. 17 of 40
   sampled FATAL TPTP problems hit it.
8. **A `tptp_pre.sh` shim** would rescue roughly 274 further translation
   failures; never built.
9. **Publication** — upstreaming to `AlgorithmicTruth/Prover9`, GPL-2 §2(a)
   per-file notices, GitHub.

## Things that have cost real time, worth not repeating

- **Never compare runs that stop on a clock.** `exit (max_seconds)` and
  `exit (max_sec_no)` cut wherever the clock landed. Two separate
  investigations into prover nondeterminism turned out to be this, and one
  "serious regression" was two runs landing either side of a 60-second limit on
  61-second work.
- **Bound runs with `ulimit -t`, never a wall clock.** A wall guard faked 38.9%
  of one sweep's timeouts under load.
- **Parallelise.** 80 cores; memory is the only limit. A serial sweep showed
  `prover9` at "1.2% CPU", which is one busy core out of eighty and reads as a
  stall.
- **Verify a scripted edit by re-reading the file.** A python edit pattern that
  forgot its `write()` reported success three times and lost four `FIXES.md`
  sections silently.
- **Measure before optimising, and re-measure after every fix.** Four of the
  performance fixes were only findable once the previous one was gone, the
  symbol-multiset quadratic turned out to be 0.12% of the run it was blamed
  for, and `predicate_elim` is 106x on a synthetic ladder and 1.12x on the real
  problem.
- **A script in `test.src` that no target invokes is documentation, not a
  check.** The audit is one line, in `test.src/Makefile`.
