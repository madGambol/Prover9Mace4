# What this work changed, measured

This is a reference fork of McCune's **LADR-2009-11A** (Prover9 and Mace4).
Commit `25b4505` is the pristine upstream tarball, unmodified, so
`git diff 25b4505` shows every change ever made here. `7f94204` adds only the
build fixes needed to compile it on a current toolchain, and is the baseline
every comparison below is measured against.

It is published for reference, not as a replacement. Everything here is
GPL-2.0-only, as upstream is.

Three documents sit beside this one:

- **`FIXES.md`** — every defect, what it was, how it was found, and the
  measurement that confirms it.
- **`ENGINE-NOTES.md`** — what the two programs teach about building an
  inference engine, which was the point of the exercise.
- **`STATUS.md`** — the state of the tree and what is still outstanding.

---

## 1. Summary

| | pristine `7f94204` | this tree |
| --- | ---: | ---: |
| **Wrong answers on 17,992 TPTP runs** | **99** | **0** |
| Crashes (SIGSEGV) on the same runs | 103 | 0 |
| Fatal errors | 421 | 139 |
| Problems that fail to translate | 3,038 | 2,948 |
| TPTP problems answered, 10 s CPU budget | 2,484 | 2,383 |
| Prover9 startup on a 44,227-clause problem | 162.8 s | 3.2 s |
| Defects found and fixed | — | 58 |

The two headline numbers point in opposite directions and both are real. **The
wrong answers are gone, and this tree answers about 4% fewer problems at a
fixed 10-second budget.** Sections 4 and 5 account for the difference
problem-by-problem; it is almost entirely the consequence of fixing a
translation defect, which turns an easy wrong problem into the hard right one.

---

## 2. The project's own test suite

None of this existed upstream. The suite is what makes the rest of the document
checkable.

```
make -C test.src tier1     # unit tests and demonstrations, about a minute
make -C test.src tier2 BASELINE=<pristine bin>   # the full regression suite
make -C test.src tier3 CORPUS=<list of inputs>   # sanitizers, poison, valgrind
make -C test.src cap   BASELINE=<pristine bin>   # the capability gate
```

| gate | what it runs | result |
| --- | --- | --- |
| tier1 | 8 unit-test binaries, an allocation audit, a cell-table test, 6 documented demonstrations, 26 translation-semantics checks | green |
| tier2 | tier1 plus 3 more binaries, **41 regression tests**, **271 Mace4 models verified clause by clause**, a determinism harness, upstream's own `test1`/`test2`/`test3` | green |
| tier3 | ASan+UBSan over **336** prover/problem pairs, a crash-poison sweep over **336**, valgrind memcheck over **168** | clean |
| cap | the 294 TPTP problems that have ever changed hands against pristine | green |

**The discrimination check matters more than the pass.** A test that passes on
the unfixed code proves nothing, so `tier2 BASELINE=` re-runs the regression
suite against the pristine build and requires that it **fail**: **33 of the 41
fail against `7f94204`** and pass here. The other 8 cover defects reachable
only under a sanitizer build or only on inputs the pristine build rejects
before reaching the defect.

The behavioural A/B over a 174-problem corpus, comparing this tree against the
previous commit rather than against pristine, reports
`prover9: identical=165 differing=2 timeout=1` and
`mace4: identical=49 differing=3 timeout=2`. The differences are accounted for
individually in `FIXES.md` under "Fixes that intentionally *can* change
results".

---

## 3. Performance

### Startup on a knowledge-base-scale problem

`Problems/CSR/CSR040-10.p` — 5.7 MB of TPTP, 44,227 clauses, 7,161 distinct
symbols after translation. Prover9 startup only (`max_given=0`, no search),
measured on an idle machine, the two builds interleaved:

| build | run 1 | run 2 | run 3 | median |
| --- | ---: | ---: | ---: | ---: |
| pristine `7f94204` | 153.9 s | 173.9 s | 162.8 s | **162.8 s** |
| this tree | 3.17 s | 3.15 s | 3.30 s | **3.17 s** |

**51x.** The instrument was checked first on a one-clause input, which measures
9 ms and 8 ms — a benchmark that cannot measure a trivial case correctly cannot
be trusted on a large one. Note pristine's 13% spread across three identical
runs on an idle machine; this tree's is 5%.

The shape matters more than the factor. Every one of the ten fixes behind this
removed a cost proportional to the **vocabulary**, or to the amount already
accumulated, rather than to the work actually being done — so the gap widens
with the size of the knowledge base. `FIXES.md` has the per-fix contribution
and a synthetic ladder for each.

### Scaling

A ladder of *n* unit clauses over 2*n* symbols, `predicate_elim` cleared,
measured idle and interleaved:

| vocabulary | pristine | this tree | factor |
| --- | ---: | ---: | ---: |
| 8,000 symbols | 5,533 ms | 198 ms | **28x** |
| 16,000 symbols | 23,789 ms | 893 ms | **27x** |
| 32,000 symbols | 111,578 ms | 4,703 ms | **24x** |

Pristine is growing about 4.5x per doubling; this tree about 4.6x. **The
constant is 24–28x smaller and the exponent is unchanged**, so at least one
quadratic remains — it is item 3 in `STATUS.md`'s outstanding list, and it is
not honest to call this fixed.

### What performance work cost

Nothing measurable in memory: `struct discrim` grew from 24 to 32 bytes, which
on CSR040-10's 287,641 nodes is 6.7 MB against 9.0 MB. The C++ containers
behind two C interfaces measured 1.04x — not a win, kept for the ~230 lines of
hand-written container code they delete, and documented as such rather than
claimed as a speedup.

---

## 4. TPTP capability, absolute

TPTP v9.3.1, 8,996-problem sample, both programs, 10 s CPU plus a 300 s re-run
of everything that ran out of time. 5,681 of the problems are first-order with
a usable SZS status, which is the denominator.

| | prover9 | mace4 |
| --- | ---: | ---: |
| proofs | 2,356 | 0 |
| models | 0 | 368 |
| exhausted (saturated, no proof) | 340 | 0 |
| **definite answers** | **2,696 (47.5%)** | **368 (6.5%)** |
| out of time | 2,703 | 3,459 |
| translate-fail | 85 | 85 |
| fatal | 53 | 163 |
| **wrong answers** | **0** | **0** |

**At least one of the two answered 2,866 of 5,681 = 50.4%.** All 368 models
were independently verified: the model is re-derived and every input clause
checked against it with `interpformat` and `clausetester`, and every clause is
satisfied in every interpretation.

A 2009 prover answering half of a 2025 problem library at a 300-second budget
is a reasonable showing, and the reason is in `ENGINE-NOTES.md` §8: Prover9
is strong on equational and algebraic problems and weak where a modern prover
would use typed arithmetic or higher-order reasoning.

---

## 5. TPTP capability, differential — the gains and the losses

Same 8,996 problems, both programs, **10 s CPU**, 4 GB per run, pristine
`7f94204` against this tree, with **both builds interleaved in one shuffled
queue**. 17,992 runs per build; every key paired. Interleaving is not
fussiness: the same binary on the same input measured 20.48, 18.95, 10.87 and
10.14 CPU seconds across four runs as machine load varied, so running one build
and then the other attributes load to the code.

| category | pristine | this tree | delta |
| --- | ---: | ---: | ---: |
| **UNSOUND — wrong answers** | **99** | **0** | **−99** |
| SIGSEGV | 103 | 0 | −103 |
| FATAL | 421 | 139 | −282 |
| translate-fail | 6,075 | 5,895 | −180 |
| CPU_KILL | 1,505 | 1,190 | −315 |
| MAX_MEGS | 1 | 463 | +462 |
| MAX_SECONDS | 6,205 | 6,838 | +633 |
| PROVED | 2,036 | 1,954 | −82 |
| MODEL | 324 | 337 | +13 |
| EXHAUSTED | 317 | 270 | −47 |

### The gain: 99 wrong answers eliminated

Pristine produced **99 wrong answers on 99 distinct problems** — mace4
reporting a model for a `Theorem` 44 times, prover9 proving a
`CounterSatisfiable` 40, mace4 on `Unsatisfiable` 8, prover9 on `Satisfiable`
7. This tree produces **none**. Of pristine's 2,776 definite answers, **99
(3.6%) were wrong**.

Every one is a mistranslation, not a prover defect, and that is established
without any reasoning: **every TPTP `fof` formula is closed, so a free variable
in the translated output is wrong on its face.** Over the 288 FOF problems
involved, pristine emits **1,168 formulas carrying a free variable** and this
tree emits **0**. The cause is defect T4: `~` binds tighter than `:` in the
parse table, so `~ ! [X] : p(X)` reached the translator as
`:(~(!([X])), p(X))`, the quantifier branch called `listterm_reverse` on
something that is not a list, got the empty list, and returned the body with
both the negation and the quantifier discarded. 1,930 of TPTP v9.3.1's 17,912
first-order problems contain the shape.

### The loss: 204 problems, and what they are

100 problems changed hands toward this tree, 204 away from it.

| pristine → this tree | count |
| --- | ---: |
| prover9 `PROVED` → `MAX_SECONDS` | 163 |
| prover9 `EXHAUSTED` → `MAX_SECONDS` | 32 |
| mace4 `MODEL` → `CPU_KILL` | 8 |
| mace4 `MODEL` → `MAX_MEGS` | 1 |

| this tree gains | count |
| --- | ---: |
| prover9 `MAX_SECONDS` → `PROVED` | 29 |
| prover9 `CPU_KILL` → `PROVED` | 15 |
| prover9 `FATAL` → `PROVED` | 14 |
| mace4 `CPU_KILL` → `MODEL` | 13 |
| **prover9 `UNSOUND` → `EXHAUSTED`** | **11** |
| prover9 `MAX_SECONDS` → `EXHAUSTED` | 6 |
| mace4 `MAX_SECONDS` → `MODEL` | 6 |
| mace4 `FATAL` → `MODEL` | 4 |
| prover9 `translate-fail` → `PROVED` | 1 |
| prover9 `CPU_KILL` → `EXHAUSTED` | 1 |

**Almost all of the loss is the translation fix, not a slower prover.** When a
pipeline changes in two places, a difference in results cannot be attributed by
looking at the results: a corrected translation is a *different* problem,
usually a harder one, and the old build was answering a formula the input never
contained. Running all four combinations of {pristine, this tree} translation ×
{pristine, this tree} prover over the 205 prover9 losses in the first run of
this A/B gave:

| | pristine prover | this tree's prover |
| --- | ---: | ---: |
| **pristine translation** | 205 answers | 190 |
| **this tree's translation** | 18 | 6 |

Per problem: **187 the input, 12 the prover, 6 noise.** Read down a column and
the input explains almost everything.

### The 12 that were the prover: a regression this work introduced

The twelve bisected to a single commit of this project's own, and are written
up as **R1** in `FIXES.md`. A per-node index made the discrimination tree
1.93x faster by dropping its children's sort order, on the correct observation
that retrieval never walks them — but the child order **is** the order of the
depth-first walk, hence the order equally-matching answers come back in, and
demodulation and forward subsumption each take the *first*. Same problem, same
47 given clauses:

```
  ordered     Generated=723      Kept=521      Forward_subsumed=201   proof in 0.03 s
  unordered   Generated=1052629  Kept=1052475  Forward_subsumed=154   2 GB exhausted after 79 s
```

Fixed by replacing the hash table with the same children **sorted in an array**
— binary-search lookup, binary-search-plus-`memmove` insert — which keeps the
1.93x and puts the invariant in the data structure instead of in a comment.
**All 12 are answered again**, at pristine's times (`SWV558-1.010` in 0.03 s,
`SWB006-10` exhausted in 3.50 s). The re-run A/B shows 9 of the 12 recovered;
the other three need 3.4–4.8 CPU seconds and straddle a 10-second budget under
64-way load, and are `EXHAUSTED` 3 times out of 3 on an idle machine.

### What the re-run says, and the part of it that is load

Re-running the whole A/B against the fixed build moves prover9 from 2,219 to
2,224 and the losses from 215 to 204, and shows **9 of R1's 12** recovered
rather than all 12. The other three are `EXHAUSTED` **3 times out of 3 on an
idle machine** at the same 10-second budget, needing 3.4 to 4.8 CPU seconds to
do it, which 64-way load inflates past ten. Comparing the two runs of the
current build also shows 15 problems "lost" and 19 "gained" — and all 15 are
answered **3 times out of 3 idle**, so they are the sweep's own noise and not
the code. **A fix measured inside a saturated sweep is measured at a
discount**, and about 15 problems of either sign is this sweep's noise floor at
a 10-second budget.

### Honest reading of the trade

Pristine answers 101 more problems at this budget **and gets 99 wrong.** This
tree answers fewer and is never wrong, never crashes, and refuses cleanly where
pristine exhausted the machine. For a tool whose output is a proof, that is the
right direction — but it is a trade, not a free win, and the 195 prover9
problems now timing out are real problems a user would rather have answered.
Two of the three causes are known and documented (the corrected translation,
and `max_megs` now being enforced where it previously was not); the remainder
is the unidentified quadratic in `STATUS.md` item 3.

### Where the MAX_MEGS came from

`MAX_MEGS` goes from 1 to 463 and this is a correctness fix showing up as a
capability loss. Mace4 had a memory limit, reported it, and did not apply it to
the allocations that mattered: the cell table, the negprop index and
ground-clause generation were all unbounded and uncharged. One problem reported
`Megabytes=524.29` against a 500 MB ceiling while holding another 568 MB, and a
sweep of such runs reached a 160 GB cgroup ceiling in twenty seconds. Those are
defects M9 through M13. Pristine's single `MAX_MEGS` run is not better
behaviour; it is a limit that was almost never consulted.

---

## 6. Provenance, and three things that went wrong in the measuring

Recorded because a summary that hides its own errors is not worth checking.

- **A build boundary inside one result file.** `bin/` was rebuilt while the
  5,171-run Mace4 300-second sweep was in flight, so rows above and below row
  3,782 come from two different binaries. All 503 model-possible problems and
  all 31 models are above it and everything below is `Theorem`/`Unsatisfiable`,
  so neither headline figure is affected — but the `FATAL`/`MAX_MEGS` split in
  that file is not from one build and must not be quoted as one figure.
- **12 rows of the final A/B were disturbed by this project's own hand.** A
  `pkill -x mace4`, run to stop an unrelated interactive process, killed 12
  in-flight sweep runs; they appeared as a verdict (`OTHER_15`) that mace4
  cannot produce. All 12 were re-run on an idle machine and all 12 fell into
  the same categories pristine gave (9 `MAX_SECONDS`, 3 `CPU_KILL`), none of
  them an answer on either build. The rows were replaced and the file now has
  no `OTHER_15`.
- **A peak-memory script was measuring the whole machine.** It sampled
  `/proc/<pid>/status` for every process matching `pgrep -x mace4`, so during a
  64-job sweep it reported 1,465 MB of peak memory for the input `p(a).`. It
  briefly made a working fix look ineffective. Now measured with `getrusage`
  via `/usr/bin/time`, and every figure re-taken. The general rule, which cost
  two mistakes to learn: **run a measuring instrument on an input whose answer
  you already know.**

Earlier in the work, a wall-clock guard on prover runs faked **38.9%** of one
sweep's timeouts by turning CPU contention into apparent slowness. Limits here
are CPU time (`ulimit -t`), not wall clock.

---

## 7. What was fixed

**58 fixes**, of which 55 carry an id in `FIXES.md` and have a reproducer and a
measurement each; the other three are translator fixes demonstrated in
`test.src/demos/` without an id.

| class | ids | count |
| --- | --- | ---: |
| memory errors (overflow, use-after-free), ASan-confirmed | M1–M8 | 8 |
| memory accounting and limits, Mace4 | M9–M13 | 5 |
| performance, all vocabulary- or accumulation-proportional | PF1–PF10 | 10 |
| TPTP translation, including the one that produced wrong answers | T1–T5, T9 | 6 |
| further translator fixes, demonstrated without an id: `include(f,[names])` ignored its name list and added every axiom; TPTP's distinct-object token was not lexable at all; NAND and NOR were never declared to the parser, so their conversion code was unreachable | `demos/t6`-`t8` | 3 |
| undefined behaviour, UBSan-confirmed | U1–U3 | 3 |
| sentinel and narrowing defects | N1–N3 | 3 |
| hangs and crashes on truncated input | D1–D2 | 2 |
| format-string defects | F1–F2 | 2 |
| suspicious constructs | S1–S5 | 5 |
| build, on a current toolchain | B1–B4 | 4 |
| the helper scripts, Python 3 | X1–X5 | 5 |
| a latent defect documented, deliberately not fixed | L1 | 1 |
| **a regression this work introduced, found and fixed** | **R1** | **1** |

Not everything is fixed. `STATUS.md` lists ten outstanding items, including an
unidentified quadratic, 70 unchecked allocations in `ladr/`, and `MAX_VARS`
still at 100 — which 17 of 40 sampled FATAL problems hit.

---

## 8. Reproducing any of it

```sh
git clone https://github.com/madGambol/Prover9Mace4.git
cd Prover9Mace4
make all                                  # builds to ./bin
git worktree add --detach /tmp/base 7f94204 && (cd /tmp/base && make all)
make -C test.src tier2 BASELINE=/tmp/base/bin
```

The TPTP work needs two more things, neither of them in this repository: the
TPTP library itself (v9.3.1, <https://www.tptp.org>) and the corpus repo, which
carries the sweep drivers, the scored results and the analysis tools. The
corpus is not on GitHub yet — it is on a Forgejo instance reachable only on the
author's tailnet, so open an issue if you need it:

```sh
git clone https://mirror.invalid/Prover9Corpus.git prover9-corpus
```

- `prover9-corpus/baseline_ab.sh` — the capability A/B, interleaved
- `prover9-corpus/cross_feed.sh` — attribute a change to the input or the prover
- `prover9-corpus/freevars.py` — the free-variable soundness oracle
- `prover9-corpus/verify_models.sh` — re-derive and check every Mace4 model
- `prover9-corpus/cap_gate.sh` — the fast capability gate
- `prover9-corpus/results/` — every scored sweep, gzipped, with its conditions

Build in a `git worktree`, not in place, and never rebuild `bin/` while a sweep
is running: both rules were learned the hard way and are recorded in
`STATUS.md`.

---

Copyright (C) 2026 Chris Arena, 2026-10-16.  Written in collaboration
with Claude Opus 5 (Anthropic).  GPL-2.0-only; see `COPYING`.  Not part
of the LADR-2009-11A release.
