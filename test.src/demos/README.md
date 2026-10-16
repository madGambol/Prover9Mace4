# Demonstrations

Each file here is a small, runnable input that demonstrates one defect found in
LADR-2009-11A and fixed in this tree. They are the worked examples behind the
write-ups in [`../../FIXES.md`](../../FIXES.md): where `FIXES.md` explains a
defect in prose, the file here lets you watch it happen.

Every file carries a comment header giving

- what the defect was,
- what the unfixed program produced,
- what the fixed program produces,
- why the difference matters,
- and the exact command to run it.

The comment syntax is the input language's own (`%` in both TPTP and LADR), so
every file is still valid input and can be fed straight to the programs.

## Running them

```sh
make -C test.src demos                  # run them all against ../bin
./test.src/demos/run.sh <bin-dir>       # or against any build
```

`run.sh` reports what each demonstration produces. Pointing it at a build of
the original code is the interesting case: that is where the defects are still
present, and the comment header in each file says what you should see.

To build the original for comparison — upstream does not compile on a modern
toolchain, so this is upstream plus only the three build fixes:

```sh
git worktree add /tmp/ladr-baseline 7f94204
make -C /tmp/ladr-baseline all
./test.src/demos/run.sh /tmp/ladr-baseline/bin
```

## Why demonstrations and not only tests

`../regress.sh` already checks every one of these mechanically, and that is
what guards against regression. These files exist for a different purpose: to
make a defect legible. Several of the defects here are invisible in ordinary
use — a translation that silently asserts the opposite of its input, a
use-after-free that only a poisoned allocator reveals — and a one-line test
assertion does not convey that. A reader who wants to confirm the claim for
themselves, or to check whether their own reasoner has the same problem,
should be able to run one file and see it.

## The files

| File | Defect | Class |
| --- | --- | --- |
| `t4-negated-quantifier.p` | `~ ! [X] : p(X)` translated to its own opposite | wrong results |
| `t7-selective-include.p` | `include(f,[names])` ignored the list and added axioms | wrong results |
| `t6-distinct-objects.p` | TPTP's `"..."` token was not lexable at all | lost problems |
| `t8-nand-nor.p` | `~&` and `~|` were never declared to the parser | lost problems |
| `t5-quoted-formula-name.p` | the translator emitted files its own reader rejects | lost problems |
| `m7-production-overflow.in` | `Op_codes` sized before the symbols that index it exist | memory safety |
| `perf-fresh-symbol.sh` | `fresh_symbol()` restarted its search on every call | performance |
| `perf-strbuf-indexing.sh` | `String_buf` indexing walked a chunk list from the head | performance |
| `perf-multiset-symbols.sh` | the symbol multiset was built, and read back, by linear search | performance |
| `perf-predicate-elim.sh` | `predicate_elimination` recomputed its whole candidate set per symbol | performance |
| `perf-sparse-features.sh` | the subsumption feature vector was sized by the symbol table, not the clause | performance / capability |
| `perf-eq-defs.sh` | `eq_defs()` accumulated dependents by walking to the tail | performance |
| `perf-arity-check.sh` | `arity_check()` compared every symbol name against every other | performance |
| `arity-ceiling.sh` | `MAX_ARITY` was 255, and the field was narrower than the parameters | wrong results / lost problems |

The remaining defects in `FIXES.md` -- the `zap_flatterm` use-after-free, the
unbounded `fscanf`, the truncated filename messages and the rest -- are checked
mechanically by `../regress.sh` rather than demonstrated here. They are
included there with the same kind of comment, but each needs either a
sanitizer build, a poisoned allocator or a generated input, so there is no
single file to read.

---

Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.  GPL-2.0-only; see
`COPYING`.  Not part of the LADR-2009-11A release.
