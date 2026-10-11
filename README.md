# Prover9 and Mace4 — LADR-2009-11A, repaired for modern toolchains

This is William McCune's **LADR-2009-11A** release of Prover9 (a resolution/
paramodulation theorem prover for first-order and equational logic) and Mace4
(a finite-model searcher for the same input language), with the changes needed
to make it build and run correctly on a current compiler.

LADR-2009-11A was McCune's last release, and on a modern toolchain it does not
compile at all. Once it does compile it carries several defects reachable from
ordinary input — including four stack buffer overflows, an uncontrolled format
string, and undefined behaviour on every default Prover9 run.

**If you just want a current Prover9, use [LADR-2026](https://prover9.org)**
(source at [AlgorithmicTruth/Prover9](https://github.com/AlgorithmicTruth/Prover9)),
the substantially revised version released in March 2026 by Jeffrey P. Machado
and Larry Lesyna. McCune's own page now recommends it for all users, and it
adds native TPTP/TSTP support, premise selection, parallel scheduling and much
more. This tree is a narrower thing: an audit of the 2009 release that stays
behaviourally identical to it. See "Relationship to LADR-2026" below.

The first commit in this repository is the **unmodified upstream tarball**, so

```sh
git diff 25b4505        # every change this tree makes to upstream
```

shows exactly what was changed and nothing else. [`FIXES.md`](FIXES.md) is the
full write-up: what each defect is, how it was found, how it was reproduced,
what the fix is, and what evidence says the fix works.

## Build

```sh
make all                 # builds 40 programs into ./bin
make test1               # prover9 smoke test   (expect a proof)
make test2               # mace4 smoke test     (expect a group table)
make test3               # the remaining programs
```

Verified with GCC 15.2 (Ubuntu), whose default `-std=gnu23` is what rejects
the original source. No configure step, no dependencies beyond libc and libm.

```sh
make -C test.src check
```

runs the test suite added here: 26 regression tests, a `String_buf` unit test,
and a Mace4 soundness check that independently re-verifies 271 reported models
against their input clauses using `interpformat` and `clausetester`.

Each regression test is written to fail against the original code and pass
after the fix, so the suite measures the fixes rather than just exercising the
programs. Run against a baseline tree — upstream with *only* the three build
fixes applied, since upstream alone does not compile — **21 of the 26 fail**.
The 5 that pass on both sides are deliberate "must not regress" controls.

## What was fixed

| | |
| --- | --- |
| **Build blockers** | K&R function definitions and implicit `int`, both hard errors under C23; `-lm` placed before the objects that need it; `make` renaming a binary over a directory that does not exist yet |
| **Memory safety** | Four stack buffer overflows reachable from input or the command line (Mace4's domain-size banner, long input filenames, `prooftrans -label`, and an unbounded `fscanf("%s")` in the proof reader); an uncontrolled format string that segfaults on input containing `%n`; every `sprintf` in the tree converted to `snprintf` with each buffer sized to its format's widest output |
| **Undefined behaviour** | Signed integer overflow in Prover9's given-clause selection, hit on *every* default run; a second instance where the overflowed value is actually used; an out-of-range `double`→`int` conversion that silently defeated `sos_limit` |
| **Hangs and crashes** | `prooftrans` and `directproof` looped forever, then dereferenced NULL, on a truncated proof |
| **Usability** | `assign(max_minutes,-1)`, `assign(max_minutes,50000000)` and `assign(pick_given_ratio,-1)` all aborted the prover on legal input |
| **Performance** | `String_buf` appends were quadratic in the string's length; appending 400 000 characters took 3.29 s and now takes under 0.01 s |
| **Tooling** | All four installed helper scripts (`attack`, `looper`, `gvizify`, `prover9-mace4`) were Python 2 and would not run; `gvizify` was also not executable |

Full detail, with reproduction cases, in [`FIXES.md`](FIXES.md).

## Does it still prove the same things?

That was the main constraint: fix the defects without changing what the prover
and the model searcher conclude. Every run was done twice, once against a
baseline build and once against this tree, and compared byte-for-byte with
timings and memory figures normalised:

- **Prover9: 165 of 168 corpus files identical** (2 differ, 1 timed out)
- **Mace4: 49 of 54 runs identical** (3 differ, 2 timed out) — 18
  model-search problems at domain sizes 2, 3 and 4, enumerating every model
- All five differences are accounted for: adding a tail pointer to
  `struct string_buf` grew it from 16 to 24 bytes, which shifts where a
  `max_megs` ceiling falls on two large problems and adds a line to the memory
  report on three others.
- An ASan + UndefinedBehaviorSanitizer build over all 168 files reports
  **0 findings** for each program. The unfixed build reports undefined
  behaviour on 135 of those 168 files.
- Separately, 408 runs of `prooftrans`/`directproof` on deliberately malformed
  proof files, under the same sanitizers: 0 findings, against 96 for the
  unfixed build.

There is **one intended output change** on ordinary input: `prooftrans` and
`directproof` now emit one blank line, not two, between a proof's comments and
its first clause. Upstream's comment-drain loop was swallowing that line and
printing it back; see "Visible output change" in `FIXES.md`.

## Relationship to LADR-2026

LADR-2026 is the active upstream and the right default for most people. It is a
much larger project — roughly 120,000 lines against upstream's 82,000 — and it
independently fixes several of the same defects this tree does, including the
C23 build errors, the `-lm` link order, the Mace4 domain-size overflow, the
quadratic `String_buf` append, and the given-clause selection overflow that
fires on every default run.

The two trees answer different questions. LADR-2026 asks what Prover9 and Mace4
should become; this tree asks what exactly was wrong with what McCune shipped,
and shows the evidence. It is six commits on top of the pristine tarball, so
every change is auditable in isolation, and the measurements in `FIXES.md` say
what each fix does and does not change.

That makes this tree useful if you want the 2009 release with its defects fixed
and nothing else altered — for reproducing published results, for packaging, or
as a base to audit.

## Scope

This is a maintenance tree, not a fork that intends to diverge. It does not
add features, change the input language, or alter the proof or model formats.

Known and deliberately left alone, both documented in `FIXES.md`:

- **L1** — `ladr/demod.c:376` discards a term its callee may have freed. It is
  reported but not fixed: the function has no callers anywhere in the tree, so
  a change to it could not be validated.
- `ladr/save/`, `bob/` and `provers.src/save/` are stale duplicate sources that
  no Makefile references. They are not compiled and no test exercises them.

## Licence

GPL-2.0-only, inherited from upstream — see [`COPYING`](COPYING) for the
licence text and [`LICENSING.md`](LICENSING.md) for how it applies to this
tree.

## Provenance

Prover9, Mace4 and the LADR library are the work of William McCune, released
from `http://www.cs.unm.edu/~mccune/prover9/`, which also hosted the HTML
manual and examples. That page now directs users to LADR-2026 at
`https://prover9.org` and no longer links the 2009 downloads; the LADR-2009-11A
tarball this tree starts from is archived by
[Software Heritage](https://www.softwareheritage.org/) and mirrored at
[theoremprover-museum/prover9](https://github.com/theoremprover-museum/prover9).

This repository is not affiliated with or endorsed by McCune, his institutions,
or the LADR-2026 maintainers.
