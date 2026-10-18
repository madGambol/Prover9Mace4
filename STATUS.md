# Where this stands

As of 2026-10-16. `master` and `rc` are the **same commit**, the tree is
clean, and every gate is green. `FIXES.md` is the defect catalogue and the
place to look for detail; this file is for picking the work back up.

## The state of the tree

| | |
| --- | --- |
| branches | `master`, `rc` — the same commit, no feature branches outstanding |
| promotion | `make promote GATED=<commit>` — `--ff-only`, and it refuses unless master's tree will be identical to the tree the gates ran on |
| upstream baseline | commit `25b4505` is the pristine 2009-11A tarball, so `git diff 25b4505` is every change |
| build | `make all`; one caveat, below, about the C++ translation units |
| gates | `make tier1`, `make tier2 BASELINE=…`, `make tier3 CORPUS=… JOBS=…`, plus the A/B |

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
make tier3 CORPUS=../../prover9-corpus/files.txt JOBS=8     # ~15 min at 8
#   ... see test.src/TIER3.md for what tier3 needs and how to read it
make ab   BASELINE=/path/to/previous/bin                     # ~20 min
```

Build the suite in a **fresh worktree**, not in a tree that has been built
before. That is what exposed `tier1` failing on a clean checkout — its
prerequisites omitted `features_test`, which its recipe runs, so it had only
ever worked because the binary was left over. `tier2` was building
`discrim_test` and never running it, and the audit meant to catch exactly this
was itself only a Makefile comment. All three are fixed; `make audit` is a
target now and `tier1` runs it first. Note the Makefiles are **not**
parallel-safe: `make -j` races in `install` and leaves a *file* named `bin`
where the directory belongs, after which every later build fails.

The pristine-plus-build-fixes baseline for `BASELINE=` is commit `04567a4`:

```sh
git worktree add --detach /tmp/base 04567a4 && (cd /tmp/base && make all)
```

That works unmodified because **every commit from the build fixes onward
carries an empty `bin/.gitkeep`**, and it is worth knowing what happens
without it, because this cost a run of the gate more than once. At a commit
with no `bin/`, the first subdirectory's install step is `cp -p <one binary>
../bin`, which with a single source and no such directory writes a 430 KB
*file* named `bin`. The next directory copies seven binaries into it and dies
on `cp: target '../bin': Not a directory`. A baseline left in that state used
to be accepted: `tier2 BASELINE=` ran against it and reported **36 of 41**
defect tests failing rather than the true 33, because three tests failed for
want of a binary rather than for want of a fix — a gate reporting a better
number than it had earned. `tier2` now refuses a baseline that is not a
directory or that lacks any binary the suite invokes.

The pristine commit `25b4505` is the exception and has no placeholder: it is
the unmodified tarball, and the 91 per-file change notices all cite
`git diff 25b4505`, so nothing may be added to it. It does not compile on a
current toolchain anyway, which is what the build fixes are for.

**Why master can skip the suite, and when it cannot.** The licence is not
that the merge fast-forwarded — it is that master's tree is byte-identical to
the tree that passed. Those differ: a `--no-ff` merge with nothing else on
master gives an identical tree and the results transfer, while a merge that
brings in a *second* branch gives a tree nothing was gated against, however
clean the history looks. `make promote` checks the identity, allows
documentation to differ because a write-up after the run changes nothing that
was tested, requires master to be an ancestor of rc, promotes with
`--ff-only`, and then proves the invariant rather than trusting it.

`tier2`'s `BASELINE=` is not optional in spirit: it asserts that at least 30 of
the 41 defect tests still **fail** against the unfixed code. A test that passes
on the unfixed code proves nothing, and that is the single mistake this suite
has made most often.

**The A/B is the gate that matters most and is the easiest to skip.** It is
`make ab BASELINE=<bin>` — `test.src/ab.sh` — and it runs **A/A first**, the
same binary against itself, because an A/B number is meaningless without the
noise floor. It stops if the floor is not zero.

It no longer trusts `comparable.txt` to say which pairs are comparable; it
decides after running each one, by looking for a clock in the exit banner.
Whether a problem finishes inside its limit **depends on machine load**, so
that cannot be a static property: re-run while a 64-job sweep was in flight,
`SYN551+2` and `SET667+3` both exit `max_seconds` and then differ against
*themselves*. A memory ceiling is not a clock — `exit (max_megs)` is
input-determined and is exactly what this gate should catch.

Expect A/A = 0 differing and A/B = 0 differing, with any clock-limited pairs
reported as skipped. `ALG033+1` and `ALG043+1` are the only corpus problems
that terminate *by* exhausting `max_megs`, so a change in allocation moves
them; both must still reach `exit (max_megs)`.

This is not belt-and-braces. Tier2 passed green on a build that had silently
**lost three proofs** (`SWC258+1`, `SET667+3`, `olsax2`); the A/B is what found
it. Tier2 is 41 problems of a few dozen clauses and structurally cannot see a
change that only matters at scale or to an invariant.

## The memory-accounting series, M9 through M12

Worth reading before touching anything that allocates. One defect appeared in
four places, and each was found only after the previous one was fixed:

| | what | how it showed |
| --- | --- | --- |
| M9 | Mace4's cell table: `d^arity` cells, `int` total, unchecked `malloc`, outside `palloc` | 56 GB request; `SIGSEGV` in 6% of a sweep |
| M10 | the negprop index: `(d+1)^arity` tree, same three faults, on by default | 160 GB cgroup ceiling in 20 s; one problem wanted 1.4 TB |
| M11 | `get_mem`/`get_cmem` above 500 pointers, and `megs_malloced()` omitting `Bytes_external` | `max_megs=500` was really holding ~1,068 MB |
| M12 | `disable_max_megs()` leaves the limit unenforced until the first `search()` node | 38 of 59 `FATAL` runs died at the OS limit, not at `max_megs` |
| M13 | `int_power`'s overflow test was undefined *and* wrong | `int_power(10,10)` returned 1410065408 as if exact; `int_power(0,5)` returned `INT_MAX` |

The pattern: **the program had a memory limit, reported it, and did not apply
it to the allocation that mattered.** M9's write-up already contained the grep
listing every direct allocation in `mace4.src`, and only the sites named by
the backtrace were acted on — which is why there were three more. Use
`test.src/audit_alloc.py`, and when a size derives from a symbol property
(arity, domain size, vocabulary count), remember that clausification
manufactures symbols no input file shows: `LCL651+1.010` has maximum arity 2
and produces Skolem functions up to arity 29.

None of these four could have been caught by the corpus A/B, which is
otherwise the strongest gate here: all four only matter at a scale the corpus
never reaches. They were found by a 5,171-problem sweep at a budget long
enough for the behaviour to appear, and by reading its failures rather than
counting them.

## The capability A/B, and what it found

**It has now been run, and it found a regression of this project's own.** The
section below this one is the design and still applies; what follows is the
result.

First run: 8,996 problems, both provers, 10 s CPU, 4 GB per run, pristine
`04567a4` against `425b83e`, both builds interleaved in one shuffled queue.
17,992 rows per build, every key paired.

| category | pristine | current | delta |
| --- | ---: | ---: | ---: |
| **UNSOUND — wrong answers** | **99** | **0** | **−99** |
| SIGSEGV | 103 | 0 | −103 |
| FATAL | 421 | 138 | −283 |
| translate-fail | 6,075 | 5,896 | −179 |
| MAX_MEGS | 1 | 485 | +484 |
| MAX_SECONDS | 6,205 | 6,839 | +634 |
| CPU_KILL | 1,505 | 1,171 | −334 |
| PROVED | 2,036 | 1,952 | −84 |
| MODEL | 324 | 338 | +14 |
| EXHAUSTED | 317 | 267 | −50 |

**The soundness result.** Pristine produced **99 wrong answers on 99 distinct
problems** — mace4 modelling a `Theorem` 44 times, prover9 proving a
`CounterSatisfiable` 40, mace4 on `Unsatisfiable` 8, prover9 on `Satisfiable`
7. This tree produces **none**. All 99 are mistranslations (T4), not prover
defects, and the proof needs no logic: every TPTP `fof` formula is closed, so a
free variable in the translated output is wrong on its face. Over the 288 FOF
problems involved, pristine emits **1,168 formulas with a free variable** and
this tree emits **0**. The LADR-2026 translator at `62ae6ff` emits the same
1,168, so the defect is live upstream — a third item for the patch proposal in
`~/Projects/Ladr-2026/REPRODUCTION.md`, alongside M10 and M12, and the most
serious of the three because it returns wrong answers rather than exceeding a
limit.

Of pristine's 2,452 definite answers, **99 (4%) were wrong**. Of this tree's,
none are.

**The capability result, and the mistake not to make with it.** prover9's sound
answers read 2,353 → 2,219, which looks like a 134-answer regression. It is
not one of that size, because the two builds are not being asked the same
question: a corrected translation is a **different** problem, usually a harder
one, and losing it is the fix working. Separating the causes needs a
**cross-feed** — all four combinations of {pristine, current} translation ×
{pristine, current} prover, which is `prover9-corpus/cross_feed.sh`:

| | pristine prover | current prover |
| --- | ---: | ---: |
| **pristine translation** | 205 answers | 190 |
| **current translation** | 18 | 6 |

Per problem: **187 of the 205 losses are the corrected input defeating both
provers, 12 are the prover, 6 were noise.** The 12 bisected to `b73f947` and
are written up as **R1** in `FIXES.md` — the child order of the discrimination
index, fixed on `feature/discrim-child-order`. Quoting the 134 would have been
wrong twice over: it overstates the regression tenfold and buries a real one.

**What the gates could not see.** tier1, tier2, the 174-run corpus A/B, the
determinism harness, ASan+UBSan on 336 pairs, poison on 336 and valgrind on
168 were all green with R1 in the tree, because each asks whether this build
still does what the last one did on inputs it already has. R1 is a capability
loss on problems none of those sets contain. **Run the capability A/B per
feature, not per release.**

**translate-fail, broken out.** 3,038 distinct problems fail to translate on
pristine and 2,948 here — 90 rescued, **0 new failures**. By TPTP language:

| | problems | in LADR's scope? |
| --- | ---: | --- |
| higher-order (TH0 1,294, TH1 362, DH0 28, DH1 17, NH0 7) | **1,708** | no — LADR is first-order |
| typed / extended first-order (TF0 759, TF1 136, TX0 107, TX1 50, NX0 102) | **1,154** | no — no type system, arithmetic or modal logic |
| untyped first-order (FOF 157, CNF 19) | **176** | **yes** |

So **56% is higher-order and 38% typed first-order**: 94% of the
"translate-fail" count is out of scope by design, not a defect. Of the 176 that
are in scope, this tree rescues **90** (75 FOF, 15 CNF). The 86 that remain:
**32 CPU_KILL** (all CSR — the translator exceeds 120 s CPU on the SUMO/Cyc
problems), **16 SIGSEGV** (13 CSR, 3 HWV — the recursive-descent parser
exhausts the 8 MB stack; `TPTP_STACK` recovers them), **20 `read_term error`**
(17 SEV, 2 BIO, 1 SYN) and **1** `Max_megs`. The 20 are a genuine open gap:
every one of `SEV607+1`'s 2,361 formulas translates alone, and the first
failing *prefix* is 180 formulas, so it is an interaction and not a syntax hole
— unresolved, see "What is outstanding".

**The narrow-index defect (T9) accounts for none of the 99.** It cannot: it
raises `fatal_error("discrim_bind_delete, cannot find end")` and exits, which
is a lost problem, never a wrong answer. Measured anyway: the largest of the
99 problems has **511 distinct symbols** against T9's 32,767 boundary. 83
problems in the sample *do* cross it (58 CSR, 22 HWV, 2 BIO, 1 NLP, the largest
at 1,254,680 symbols), and none of them reached a verdict on either build — 74
translate-fail both sides, 5 translate-fail → CPU_KILL, 4 CPU_KILL both sides.
T9's value is for knowledge-base-scale input fed directly, not for TPTP.

## The design of the A/B, and what was previously unmeasured

Every other TPTP figure in this document is **absolute, not differential**: it
was produced by a fixed build, so it says what this tree can do, not what the
work added. Before the A/B above, only three things were established against
pristine:

- **33 of 41** defect tests fail on pristine `04567a4` and pass here (tier2's
  `BASELINE=` gate).
- Startup on `CSR040-10.p` went from **157.53 s to 3.80 s**, a factor of 41.
- M9–M13's reproductions: pristine crashes or exhausts the OS where this tree
  exits cleanly with a diagnostic.

The capability A/B — same sample, same budget, pristine against current, with
the categories broken out — is `prover9-corpus/baseline_ab.sh`, scored with
`cap_compare.sh`. It interleaves the two builds in one queue rather than
running one then the other, because CPU time is **not** load-invariant and a
sequential run would hand them different conditions; see "CPU time is not
load-invariant" in `FIXES.md`. Both sides run under the same memory cap, which
is part of the condition and must not be changed mid-run.

Expect it to cut both ways. The PF2–PF10 startup work should convert timeouts
into answers at a fixed budget. But M9, M10 and M12 make Mace4 **honour**
`max_megs` where it used to exceed it — the LADR-2026 reproduction showed
upstream answering a problem at 456 MB against a stated 20 MB ceiling, where
this tree declines it — so some answers should be lost. A net figure would
hide that, which is why `cap_compare.sh` reports which problems changed hands
and why.

## What is outstanding

Roughly in order of expected value.

0. **The capability A/B is now part of the gate, in principle and not yet in
   the Makefile.** R1 is the argument: a change that keeps every existing test
   green can still cost twelve problems, and nothing short of running the
   pristine comparison sees it. `make ab BASELINE=...` exists for the 174-run
   corpus; what R1 needed was the 8,996-problem sample, which is an hour at 64
   jobs. A per-feature tier between tier2 and the full sweep — say the 310
   problems that have ever changed hands, plus R1's twelve — would have caught it in
   minutes, and `prover9-corpus/cap_gate.sh` is that gate: 294 problems, both
   provers, interleaved, reporting 10 lost against the unfixed build and 0
   against the fixed one.

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
   10x would add much less.

   **Mace4 at 300 s found two real defects before it produced a figure.** At
   10 s no run reaches a domain size where it matters; at 300 s many do. 6%
   of runs were dying in `SIGSEGV` and the sweep drove a cgroup to its
   ceiling — M9 and M10, the cell table and the negprop index, both
   unbounded, unchecked and unaccounted. With those fixed the sweep ran to
   completion: **5,171 of 5,171, 0 SIGSEGV, 0 UNSOUND**, through
   `oracle_sweep_resume.sh`.

   Mace4 goes from 5.9% to **6.5%**, and the two provers between them from
   41.4% to **50.4%** of the 5,681 first-order problems with a usable SZS
   status. All 503 problems whose stated status allows a model were
   attempted, so that figure is complete at this budget; the remaining 4,668
   are `Theorem`/`Unsatisfiable`/`ContradictoryAxioms`, where a model cannot
   exist and the only possible finding is a violation. The 31 models were
   each re-derived and checked with `interpformat` and `clausetester` —
   every clause satisfied in every interpretation (`verify_models.sh`).

   Two caveats recorded with the data rather than buried: the results file
   has a **build boundary at row 3782** because `bin/` was rebuilt mid-sweep
   (it touches neither headline figure, but the `FATAL`/`MAX_MEGS` split is
   not uniform across the file), and these are figures for Mace4 *honouring*
   `max_megs=2000`, which the pristine binary did not.
5. **The remaining unchecked allocations in `ladr/`.** The important one is
   fixed: M11 charged and checked `get_mem`/`get_cmem`'s large-block path,
   which every caller goes through, and put `Bytes_external` into
   `megs_malloced()`. About seventy direct `malloc`/`calloc` sites in `ladr/`
   still do not check their return — `interp.c` has roughly forty of them.
   None is exponential in an arity the way M9 and M10 were, so none can take
   a machine down, and the sizes are now mostly charged because they come
   through `get_mem`; what is left is turning a NULL dereference into a
   diagnostic. `test.src/audit_alloc.py` lists them — run it, do not grep,
   because grepping is what left M10 and M11 in place after M9.

   Current counts: `mace4.src` 13 sites, **0 unchecked**; `ladr/` 79 sites,
   70 unchecked (about forty in `interp.c` alone); `provers.src` 14 sites, 7
   unchecked.

   Two specific findings from that audit, neither urgent:

   - `compile_interp` in `ladr/interp.c` computes `n = domain_size^arity`
     with a plain `int` multiply loop and **no overflow guard**, then
     `malloc(n * sizeof(int))` unchecked. It is the same wrap as M9, but
     shielded in practice: `listterm_length(lst) != n` two lines above means
     the input has to supply `n` list elements, so a wrapped `n` fails that
     check and a large `n` needs a correspondingly huge input file. The arity
     is not manufactured by clausification here, which is what made M9 and
     M10 dangerous. Worth a `cells_for_symbol`-style guard anyway, since this
     is the path `interpformat` and `clausetester` use — the soundness
     oracle's own verification tools.
   - `init_pair_index` in `ladr/pindex.c` allocates `n*n` twice over, but
     **has no callers**, so it is dead rather than dangerous.
6. **Two more hand-rolled containers**, if the C++ pattern is worth continuing:
   the discrimination-tree child index and the grouping in `collect_multiples`.
   Expect ~0% speed and roughly 230 fewer lines; the case is bug surface, not
   performance.
7. ~~**`max_megs` default.**~~ **Done, and it had to be.** Both overrides are
   1000 now. M11 made the accounting honest, which by itself would have
   halved the effective budget every earlier result was obtained under — the
   500 MB ceiling was really holding about 1,068 MB on `ALG033+1`. Doubling
   restores the budget and makes the number mean what it says.
8. **`MAX_VARS`** is still 100, blocked by the `Context` arrays, the variable-ID
   stride multiplier, `compress.c`'s char assumption and `MAX_BASIS`. 17 of 40
   sampled FATAL TPTP problems hit it.
9. **The 20 `read_term error` translation failures** (17 SEV, 2 BIO, 1 SYN).
   Every one of `SEV607+1`'s 2,361 formulas translates on its own; the first
   failing prefix is 180 formulas, so it is an interaction, not a syntax hole.
   These are Coq-exported set theory with quoted names containing `$`, `.` and
   escaped quotes, all of which parse individually. Reproduce with
   `prover9-corpus` and the binary search in the session notes.

10. **A `tptp_pre.sh` shim** would rescue roughly 274 further translation
   failures; never built.

11. **Publication** — done, with one piece deliberately dropped. The canonical
   home of this tree is <https://github.com/madGambol/Prover9Mace4>, with a
   private mirror kept in step; `master` and `rc` are aligned on both. The
   corpus is on the private mirror only, so it is unreachable to anyone
   arriving from GitHub — publishing it is the one loose end here.
   GPL-2 §2(a) per-file change notices are in place — 91 files, see
   `LICENSING.md` for the convention and for what was deliberately left alone.
   Upstreaming to `AlgorithmicTruth/Prover9` was **dropped**: without a total
   summary showing a real net improvement it does not earn a pull request, and
   `RESULTS.md` reports a net loss of answers at a fixed budget. The three
   fixes that would have gone up (T4, M10, M12) are prepared on a branch in
   `~/Projects/Ladr-2026/src` with `PR.md` beside it, if that ever changes.

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

---

Copyright (C) 2026 Chris Arena, 2026-10-16.  Written in collaboration
with Claude Opus 5 (Anthropic).  GPL-2.0-only; see `COPYING`.  Not part
of the LADR-2009-11A release.
