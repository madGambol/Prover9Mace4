# Prover9 and Mace4, for engineers building a reasoner

Notes for engineers building a knowledge-base inference engine, written from
an audit of McCune's **LADR-2009-11A** release (Prover9 and Mace4) and a run of
both over the **TPTP v9.3.1** problem library.

Two things are on offer here. The first is a set of design ideas that a
knowledge-base reasoner can borrow, with the real option names and the source
lines they come from. The second is a measured capability baseline over a
labelled corpus, so another engine's results can be compared against a
well-understood classical prover on the same problems.

Everything below was either read out of the source or measured on this
machine. Nothing is estimated; where something was not established the text
says so, and the appendix separates what was measured from what was not.

Line references are to the repaired tree in this repository, whose
first commit is the untouched 2009 tarball, so `git diff 25b4505` shows every
change.

---

## 1. What these programs actually are

Neither program is a chaining engine, and that is the first thing to get
straight, because it changes which of their ideas transfer.

**Prover9 is a saturation prover.** It runs the given-clause loop (the "Otter
loop"): clauses are split between a *set of support* (`sos`) and a *usable*
list; one clause is selected as the **given** clause; it is resolved and
paramodulated against the usable list; the results are simplified, weighed,
subsumption-checked and appended to `sos`; repeat until a contradiction is
derived or the search space is exhausted. There is no goal stack and no
agenda of rules to fire. Direction of search is an emergent property of which
clauses are in `sos` and which inference rules are enabled.

**Mace4 is a finite model searcher.** It fixes a domain size *n*, grounds the
clauses over the *n* elements, and does a backtracking search with propagation
over the resulting function and predicate tables — a cell in the table for
`f(2,3)` is a search variable. It does no chaining at all. Termination is
free: the search space is finite by construction.

The two are complementary in a way that matters for a KB. A saturating prover
can tell you a query follows. Only a model finder can hand you a witness that
it does **not**.

---

## 2. Forward and backward chaining

### Prover9 has an explicit forward-chaining mode

`set(production)` turns Prover9 into a production rule system. From
`provers.src/search.c:447`:

```c
flag_flag_dependency(p->production, TRUE, p->raw,              TRUE);
flag_flag_dependency(p->production, TRUE, p->eval_rewrite,     TRUE);
flag_flag_dependency(p->production, TRUE, p->hyper_resolution, TRUE);
flag_flag_dependency(p->production, TRUE, p->back_subsume,    FALSE);
```

Read that as a four-part recipe: fire rules on facts (hyperresolution),
evaluate built-in conditions in rule bodies (`eval_rewrite`, which is what
makes `$`-terms like `$sum`, `$less`, `max`, `mod` compute), do not reorder or
preprocess the clauses (`raw`), and do not back-subsume, so derived facts
persist rather than being discarded when a more general one appears.

That is a forward chainer assembled out of saturation parts.

### The inference rules are the FC/BC analogues

All of these are real flags, verified present in `search.c`:

| Flag | Reading |
| --- | --- |
| `set(hyper_resolution)`, `set(pos_hyper_resolution)` | forward: a nucleus with negative literals is resolved against all-positive satellites, yielding new positive facts |
| `set(neg_hyper_resolution)` | backward: the dual, driven from the negated goal |
| `set(ur_resolution)`, `set(pos_ur_resolution)`, `set(neg_ur_resolution)` | unit-resulting resolution — FC or BC restricted to units |
| `set(binary_resolution)`, `set(neg_binary_resolution)` | general, either polarity |
| `set(paramodulation)`, `set(ordered_para)`, `set(basic_paramodulation)`, `set(para_units_only)`, `set(para_from_small)`, `set(para_into_vars)`, `set(para_from_vars)` | equality reasoning, with the restrictions that keep it finite |
| `set(factor)`, `set(unit_deletion)`, `set(back_demod)` | simplification |

So "which direction do you chain?" is answered by choosing an inference rule,
not by a control loop that alternates.

### The set of support is what really directs the search

Direction in practice comes from `sos` membership, tuned by
`assign(sos_limit, n)`, `assign(min_sos_limit, n)`,
`assign(sos_keep_factor, n)` and `set(restrict_denials)`. Put the axioms in
`sos` and the search runs data-driven; put the negated goal there and it runs
goal-driven. `assign(sos_limit, -1)` means no limit; a positive limit makes
Prover9 discard the heaviest clauses when `sos` grows past it, which is the
blunt instrument for keeping a large axiom set tractable.

For a KB engine the lesson is that "forward" and "backward" need not be two
code paths. They can be one saturation loop plus a policy about which formulas
are allowed to initiate inference.

---

## 3. The recursion problem, solved structurally

An engine may alternate single-step forward and backward chaining to
avoid getting stuck on a rule applied to its own output. Prover9 does not need
that alternation, because it makes the loop impossible rather than detecting
it.

**Reduction orderings.** An equation used as a rewrite rule (a *demodulator*)
is applied only in the direction that makes the term **smaller in a
well-founded order**. `assign(order, ...)` accepts exactly three values, from
`provers.src/search.c:252`:

```c
p->order = init_stringparm("order", 3, "lpo", "rpo", "kbo");
```

- `lpo` — lexicographic path ordering
- `rpo` — recursive path ordering
- `kbo` — Knuth–Bendix ordering

Because every rewrite strictly decreases a well-founded measure, a rewrite
sequence cannot cycle. There is no loop to detect. `set(ordered_para)` and
`set(basic_paramodulation)` apply the same discipline to paramodulation, so a
rule cannot feed itself through the equality machinery either.

**Where orderings cannot decide.** Some comparisons depend on the instance,
not the rule — the two sides are ordered one way for some substitutions and
the other way for others. Prover9 handles those explicitly with
`set(lex_dep_demod)` and `assign(lex_dep_demod_lim, n)` (and
`set(lex_dep_demod_sane)`), which allow such demodulators under a bounded
number of steps rather than refusing them.

**Backstops.** For everything the ordering does not cover:

| Control | Effect |
| --- | --- |
| `assign(demod_step_limit, n)` | cap rewrite steps per clause |
| `assign(demod_increase_limit, n)` | refuse a rewrite that grows the term past a bound |
| `assign(max_weight, n)` | discard any derived clause heavier than *n* — this is what kills output-growth loops |
| `assign(max_depth, n)` | cap term depth |
| `assign(max_literals, n)` | cap clause length |

### The transferable recommendation

For rules that can recurse on their own output: define a
reduction ordering over the relevant function symbols and fire those rules
only in the decreasing direction, with a weight cap as the safety net. That
converts a heuristic dodge into a termination guarantee.

Two honest caveats. First, it only buys termination for the part of the system
you can express as rewriting — ordered rewriting of equalities and of rules
whose conclusion is a term replacement. Rules that genuinely derive new
relational facts are not rewrites and need the saturation-side limits
(`max_weight`, `sos_limit`) instead. Second, choosing the symbol precedence
that induces a useful ordering is itself work; a bad precedence terminates but
rewrites in an unhelpful direction. Prover9 picks one automatically under
`set(auto)` and lets you override it, which is probably the right shape for a
KB too: a default heuristic plus an escape hatch.

---

## 4. The closest counterpart to an alternating engine

Prover9 does interleave, but over *selection criteria* rather than inference
directions. `assign(pick_given_ratio, n)` is declared at `search.c:203` and
expands, at `search.c:302–306`, into a set of "parts":

```c
parm_parm_dependency(p->pick_given_ratio, p->age_part,    1, FALSE);
parm_parm_dependency(p->pick_given_ratio, p->weight_part, 1,  TRUE);
parm_parm_dependency(p->pick_given_ratio, p->false_part,  0, FALSE);
parm_parm_dependency(p->pick_given_ratio, p->true_part,   0, FALSE);
parm_parm_dependency(p->pick_given_ratio, p->random_part, 0, FALSE);
```

So `pick_given_ratio` sets `age_part = 1` and `weight_part = n`: pick the
oldest clause once for every *n* times you pick the lightest. The other parts
(`false_part`, `true_part`, `random_part`) select by truth in a supplied
interpretation, and at random, and default to zero.

`provers.src/giv_select.c` implements the rotation. `get_given_clause2()`
asks `next_selector(&High)`, falling back to `next_selector(&Low)`, and takes
`avl_smallest()` from that selector's index (or a random element if the
selector's order is `GS_ORDER_RANDOM`). `iterations_to_selection()` schedules
which part is due.

The motivation is the same as your FC/BC alternation: fairness, so that one
criterion cannot starve the search. Pure best-first on weight will chase a
single small-clause lineage forever; mixing in age guarantees every clause is
eventually selected, which is what makes the procedure fair and therefore
complete in the limit. The design difference worth noting is that it is a
**ratio**, not a strict alternation, and it is declarative — one parameter,
not a control structure in the loop.

If such an alternation exists partly to guarantee fairness rather than
only to dodge recursion, a weighted round-robin over selection criteria is
likely a better fit than strict 1:1 alternation, because the ratio is then a
tuning knob rather than a fixed policy.

---

## 5. Hints — the highest-leverage idea here

Bob Veroff's proof-reuse method, integrated into Prover9 as *hints*. You
supply clauses from previous proofs; the search is steered toward clauses that
match them. The controls:

| Option | Effect |
| --- | --- |
| `assign(hints_part, n)` | give hint-matching clauses their own share of given-clause selections |
| `set(degrade_hints)` | reduce a hint's pull as it is used, so the search does not get pinned to one old proof |
| `set(back_demod_hints)` | keep hints rewritten as demodulators are derived |
| `set(breadth_first_hints)` | process hint-matchers breadth-first |
| `assign(limit_hint_matchers, n)` | bound how many matchers are retained |
| `set(collect_hint_labels)` | report which hints were used |

For a KB that answers many structurally similar queries this is probably the
single most valuable idea in the system. The reasoning cost of a query is
dominated by search, and a KB's query stream is highly repetitive: the same
lemmas get re-derived constantly. Hints turn "we proved something like this
last week" into search guidance rather than a cache lookup, which is strictly
more general than memoising answers — a hint still helps when the new query is
only *similar*, where a memoised answer does not apply at all.

The `degrade_hints` detail is worth copying too. Without it, proof reuse
degenerates into overfitting on the first proof found.

---

## 6. Mace4 as an oracle

Mace4 answers a question a saturating prover structurally cannot: it produces
a **finite countermodel**, a concrete witness that a query does not follow from
the KB.

Verified behaviour: Mace4 negates `formulas(goals)` and searches for a model of
assumptions-plus-negated-goal. Tested directly — a trivially valid goal
(`P(a)` from `P(a)`) yields "Exiting with failure", no model; an invalid goal
(`Q(b)` from `P(a)`) yields a model. So "Mace4 found a model" means "the goal
does not follow", and for a KB that is a counterexample you can show a user.

Its controls:

| Group | Options |
| --- | --- |
| Domain sizes | `domain_size`, `end_size`, `increment`, `iterate_up_to`, `iterate_primes`, `iterate_nonprimes` |
| Stopping | `max_models`, `max_seconds`, `max_seconds_per`, `max_megs` |
| Symmetry | `lnh` — the least number heuristic, which breaks symmetry among interchangeable domain elements |
| Search policy | `selection_measure`, `order_domain` |
| Propagation | `negprop`, `neg_assign`, `neg_assign_near`, `neg_elim`, `neg_elim_near` |
| Built-ins | `arithmetic`, `integer_ring` |
| Output | `print_models`, `print_models_tabular`, `return_models`, `report_stderr` |

Its exit codes are informative enough to drive a harness, from
`mace4.src/msearch.h:137`: `0` max_models reached, `2` exhausted (no model at
the sizes tried), `3` all models found, `4`/`5` out of time with/without
models, `6`/`7` out of memory with/without models, `102` SIGSEGV.

Two things transfer directly:

- **Consistency checking.** Run the model finder on a KB fragment alone. A
  model is evidence of consistency at that domain size; exhaustion at
  increasing sizes is evidence (not proof) of inconsistency, and a derived
  contradiction from the prover is the proof.
- **`lnh`-style symmetry breaking.** Any engine that grounds over a finite set
  of individuals will generate symmetric search branches that differ only by
  renaming. The least-number heuristic — require that a newly introduced
  element is the smallest unused one — removes most of them for almost no
  cost. This is a general grounding technique, not a Mace4 peculiarity.

For comparison purposes, note that a model finder also makes a *soundness
oracle* possible, which is how the TPTP comparison below is structured.

---

## 7. Reproducing the comparison

### The shared corpus

The TPTP library is in neither this repository nor the corpus repository — it
is fetched separately from <https://www.tptp.org> — and the scripts in both
take its location from **`TPTP_DIR`**, which names the distribution root. Keep
the copy read-only and outside any single project so several can share it. The
provenance of the copy these numbers were measured against:

- **TPTP v9.3.1**, distribution dated 21 August 2026, 10 GB unpacked.
- `sha256 8e9462d9443a2250000224cf4f7002a64ea02cbe07fa5cdcdecfd0fe8cd81f03`
  for the 890.7 MB archive, which is kept alongside so the copy can be
  re-verified.
- Verified before unpacking: gzip CRC good, one top-level directory, and no
  absolute paths or `..` components among 30,367 entries.

Problem counts, by the form encoded in the filename:

| Mark | Form | Count |
| --- | --- | --- |
| `+` | FOF, first-order quantified formulae | 9,436 |
| `-` | CNF, clause normal form | 8,476 |
| `^` | THF, higher-order | 5,279 |
| `_` | TFF, typed first-order | 3,799 |
| | **total** | **26,990** |

Prover9's `tptp_to_ladr` understands `fof`, `cnf`, `input_clause` and
`include`, and knows nothing of the typed or higher-order dialects, which
postdate it. So the comparable set is the **17,912** FOF and CNF problems.

### The labelled oracle

Every TPTP problem header declares the answer:

```
% Status   : Theorem
```

Over those 17,912 problems:

| Status | Count | Meaning |
| --- | --- | --- |
| `Theorem` | 7,246 | the conjecture does follow from the axioms |
| `Unsatisfiable` | 6,709 | the clause set has no model |
| `Satisfiable` | 1,673 | the clause set has a model |
| `CounterSatisfiable` | 888 | the conjecture does **not** follow |
| `ContradictoryAxioms` | 557 | the axioms alone are inconsistent |
| `Unknown` | 807 | no oracle |
| `Open` | 32 | no oracle — genuinely open |

**17,073** carry a decisive label. That is what turns benchmarking from "did
the output change?" into "was the answer right?", and it is the reason this
corpus is worth the disk space for any engine under comparison.

Partitioned by what each tool can be asked to do:

- **model-possible** (`Satisfiable` + `CounterSatisfiable`) = **2,561**. Mace4's
  actual job.
- **no-model** (`Theorem` + `Unsatisfiable` + `ContradictoryAxioms`) = **14,512**.
  Prover9's job; Mace4 can only be probed here for soundness, since it can
  never legitimately succeed.

### The soundness rule

| Observation | Verdict |
| --- | --- |
| Prover9 proves a problem labelled `Satisfiable` or `CounterSatisfiable` | **unsound** — it claims entailment where none exists |
| Mace4 finds a model for a problem labelled `Theorem`, `Unsatisfiable` or `ContradictoryAxioms` | **unsound** — it claims a countermodel where none exists |

### Mechanics

`include('Axioms/...')` paths are relative to the distribution root, so run
everything with that as the working directory:

```sh
cd "$TPTP_DIR"
tptp_to_ladr < Problems/PUZ/PUZ001+1.p > /tmp/p.in
prover9 -f /tmp/p.in
```

Three measurement points that cost us time and are worth inheriting:

1. **Use CPU-time limits, not wall-clock.** Prover9 and Mace4 enforce
   `assign(max_seconds, n)` against user CPU time. An outer wall-clock guard
   turns CPU contention into spurious timeouts: at 80-way parallelism on 80
   cores, **38.9%** of our first run's "timeouts" had been killed by the wall
   guard rather than by the real limit, which understates the prover. Enforce
   the backstop with `ulimit -t` and leave the wall guard far out, for hangs
   only.
2. **Raise `max_megs`.** Prover9's and Mace4's default is 500 MB, a 2009-era
   figure, and left alone it — not the clock — is the binding constraint on a
   good number of problems: `ALG021+1` hits it in about 4 seconds. Measure the
   prover, not the default.

   Note *which* 500 to change, because there are two and only one of them
   matters here. `DEFAULT_MAX_MEGS` in `ladr/memory.c` is the library's
   fallback, used by programs that never call `set_max_megs()` — this tree
   raises it to 1000, which is what let the oversized `tptp_to_ladr`
   translations through, since that program has no option to ask for more. The
   provers override it from their own parameter,
   `init_parm("max_megs", 500, ...)` in `provers.src/search.c` and
   `mace4.src/msearch.c`, so for them the effective default is still 500 and
   only `assign(max_megs, n)` changes it. Verified rather than assumed:
   prover9 on `ALG021+1` still exits `max_megs` with its default and does not
   with an explicit `assign(max_megs, 1000)`.
3. **Classify by exit code, not by output text.** Prover9's codes, from
   `provers.src/search-structures.h:282`: `0` proof found, `1` fatal, `2`
   `sos_empty` (search space exhausted with no proof — a real answer, not a
   timeout), `3` out of memory, `4` out of time, `5` max_given, `6` max_kept,
   `7` action, `102` SIGSEGV. Lumping 2, 3, 4 and 102 together as "failed"
   throws away most of the diagnostic value.

---

## 8. Capability profile

Budgets for the run reported here: Prover9 30 s CPU and 2 GB, with a rework
pass at 90 s and 4 GB over the 3,000 lowest-rated unsolved problems; Mace4
30 s CPU where a model can exist and 5 s elsewhere as a soundness probe. All
limits CPU time, 60 of 80 cores, with the corrected translator (§10) in force.

### Headline

| | attempted | solved | rate |
| --- | --- | --- | --- |
| Prover9, on `Theorem` / `Unsatisfiable` / `ContradictoryAxioms` | 14,070 | 6,423 | 45.7% |
| Mace4, on `Satisfiable` / `CounterSatisfiable` | 2,532 | 1,056 | 41.7% |

Soundness violations: 0.

Two notes on how a "solve" is counted, because the obvious rule is wrong in
both directions. Prover9 can report `Exiting with N proof` and still exit with
`SOS_EMPTY`, so classifying on the exit code alone understated the count by 27
`Unsatisfiable` problems where deriving a contradiction *is* the whole task.
Conversely, 60 `Theorem` problems produce a proof while Prover9 still reports
`SEARCH FAILED` and never prints `THEOREM PROVED`; the proof does reference
the conjecture, so the likeliest reading is a conjecture that clausifies into
several denials of which only some were proved. Those 60 are **not** counted as
solved and appear as `PARTIAL_PROOF` in the raw results. We did not confirm
that mechanism, only that a proof exists and that Prover9 itself declines to
call the problem proved.

Time to solve, Prover9: median 0.13s, 90th percentile 15.44s, and 71%
of all solves land within one second. The shape of that distribution is the
practically important part — a classical prover either gets there quickly or
not at all, which is why a KB can afford to run one speculatively with a short
budget.

### Difficulty curve, and why two of them

TPTP's `Rating` field is the fraction of rating-eligible provers that *fail* a
problem: `0.00` means everyone solves it, `1.00` means nobody does. The
ratings are recomputed against contemporary provers, so scoring a 2009 prover
against today's ratings understates it.

We therefore recorded both the current rating and the rating from TPTP
**v4.0.x**, which is contemporary with LADR-2009-11A. Of the 17,912 problems,
**8,986** existed then and carry a v4.0.x rating. For exactly those problems
the mean rating is **0.257 today against 0.476 then** — modern provers are
roughly twice as effective on the same set. Any claim about Prover9's standing
needs to say which scale it is on.

Rating today:

| Band | solved / attempted | rate |
| --- | --- | --- |
| 0.00 | 2844 / 3394 | 83.8% |
| 0.00–0.20 | 1801 / 2360 | 76.3% |
| 0.20–0.40 | 1469 / 2969 | 49.5% |
| 0.40–0.60 | 320 / 1693 | 18.9% |
| 0.60–0.80 | 71 / 1582 | 4.5% |
| 0.80–1.00 | 19 / 1618 | 1.2% |
| 1.00 | 18 / 1212 | 1.5% |

Rating in v4.0.x, Prover9's own era:

| Band | solved / attempted | rate |
| --- | --- | --- |
| 0.00–0.20 | 1590 / 1816 | 87.6% |
| 0.20–0.40 | 1185 / 1483 | 79.9% |
| 0.40–0.60 | 671 / 1177 | 57.0% |
| 0.60–0.80 | 311 / 1457 | 21.3% |
| 0.80–1.00 | 60 / 1222 | 4.9% |
| 1.00 | 3 / 423 | 0.7% |

Problems solved despite a current rating of 0.50 or worse: 217.

### By form

| Form | solved / attempted | rate |
| --- | --- | --- |
| FOF | 2702 / 7596 | 35.6% |
| CNF | 3721 / 6474 | 57.5% |

### By domain

Full table in the generated report. The domains that bear most on a
commonsense KB, with their FOF+CNF problem counts and names taken from
`Documents/OverallSynopsis`:

| Code | Domain | FOF+CNF problems | Prover9 solved |
| --- | --- | --- | --- |
| CSR | Commonsense Reasoning | 956 | 32 / 548 — 5.8% |
| KRS | Knowledge Representation | 298 | 145 / 173 — 83.8% |
| NLP | Natural Language Processing | 562 | 32 / 44 — 72.7% |
| SWB | Semantic Web | 229 | 50 / 146 — 34.2% |
| MGT | Management | 208 | 161 / 171 — 94.2% |
| AGT | Agents | 54 | 16 / 52 — 30.8% |
| PLA | Planning | 82 | 28 / 58 — 48.3% |

CSR is the closest analogue to a knowledge base's own problem shape: large,
hand-built ontological axiom sets with shallow but broad inference. It is also
where the scale limits in §9 bite hardest, so the CSR row is the one to read
first.

For context, the largest domains overall are SWV (Software Verification, 1,356),
SET (1,268), LCL (Logic Calculi, 1,262), SYN (Syntactic, 1,235), GRP (Groups,
1,166) and NUM (Number Theory, 1,094). A classical prover's strengths cluster in
the algebraic domains; that is the comparison baseline, not the commonsense one.

### By problem size

| Formulae / clauses | solved / attempted | rate |
| --- | --- | --- |
| ≤ 10 | 1965 / 2715 | 72.4% |
| 11–50 | 2506 / 4257 | 58.9% |
| 51–200 | 1267 / 3253 | 38.9% |
| 201–1,000 | 528 / 1730 | 30.5% |
| > 1,000 | 157 / 2115 | 7.4% |

---

## 9. Scale limits — the most relevant lesson

This is the part that transfers most directly to a KB engine, and it is mostly
bad news about the 2009 toolchain.

**The problems are KB-sized.** `Problems/CSR/CSR029+5.p` declares **540,250
formulae** in its header, pulled in through `include('Axioms/CSR002+4.ax')`.
That is an industrial-ontology-scale axiom set, in TPTP precisely so that
large-theory reasoning can be benchmarked.

**The translator does not survive them.** Measured behaviour of
`tptp_to_ladr`, all on this machine. The stack and formula-size rows were
measured directly in the course of the sweep; the scaling rows, the arity
ceiling and the memory cap were measured by a sub-agent tasked with
characterising the translation failures, and the causes it names are cited to
specific source lines, which we have checked, but we did not independently
re-run its timing measurements:

| Limit | Measurement |
| --- | --- |
| Recursive parser vs. default stack | segfaults on the largest problems with the default 8 MB stack; the sweep raises it to 1 GB. We observed 58 `Segmentation fault (core dumped)` events before raising it. |
| Quadratic parse of one large formula | 4,000 / 8,000 / 16,000 / 32,000 disjuncts take 0.22 / 1.53 / 7.14 / 25.67 s |
| Cubic renaming of quoted symbols | 200 / 400 / 800 / 1,600 quoted symbols take 0.16 / 1.11 / 7.92 / **62.96 s**. Cause: `fresh_symbol()` (`ladr/symbols.c:2722`) restarts its search from `ladr0` on every call, and `str_exists()` (`ladr/symbols.c:678`) scans the whole symbol table. With `-q`, which bypasses `fresh_symbol`, 1,600 take 0.24 s. `Axioms/CSR003+2.ax` has 14,265 such symbols; `Axioms/BIO001+0.ax` has 25,062. |
| Quadratic symbol gathering | 32k / 64k / 150k distinct symbols cost 3.0 / 12.6 / 73.4 s, against 1.0 s for the same file size with one symbol. Cause: `multiset_add_n()` (`ladr/glist.c:1930`) is a linear, non-tail-recursive scan called once per symbol *occurrence*. Its recursion depth equals the symbol count, so at 300k+ symbols it is also a stack-overflow candidate. |
| Arity ceiling | `MAX_ARITY` is `UCHAR_MAX` = **255** (`ladr/term.h:77`). Seven problems carry Skolem functions of arity up to 280 and simply cannot be represented. |
| Translator memory cap | `tptp_to_ladr` parses only `-q` and never calls `set_max_megs()`, so it dies at the 500 MB default while `prover9` and `mace4` both accept `assign(max_megs, n)`. Three HWV problems of 278–298 MB hit this. |

### What has since been fixed in this tree, and what it cost to find

Four of the limits above are gone, and finding them produced two further
structural lessons that matter more for a KB engine than anything in the table.
Full detail, with measurements, is in `FIXES.md` as PF2 through PF6.

| Limit | Status |
| --- | --- |
| Cubic renaming of quoted symbols | **fixed** — `fresh_symbol()` keeps a per-prefix high-water mark; 440x at 1,600 symbols |
| Quadratic symbol gathering | **fixed** — `multiset_add_n()`'s linear scan replaced by a hash-backed accumulator (`I2accum`) on the unbounded paths; `lex_order()` indexes the multiset once instead of searching it per symbol |
| Recursion depth tied to input size | **fixed for lists** — all 40 self-recursive list functions in `glist.c` are loops; 10 of 12 probed were crashing at 400,000 elements. The *parser*'s recursion over formula structure is unchanged |
| Arity ceiling 255 | **fixed** — a named `ArityType` with a ceiling of 65,534; 5 of the 7 blocked problems now translate |
| `predicate_elimination` O(R³) | **fixed** — quadratic now; 106x at 8,000 symbols on a synthetic ladder |
| Feature vector sized by the symbol table | **fixed** — sparse now; 184x less memory and 232x less time at 12,000 symbols, and it no longer runs out of memory where it used to |
| `eq_defs()` accumulating dependents by walking to the tail | **fixed** — 1.07 billion cell visits removed; 2.05x |
| `arity_check()` comparing every symbol name against every other | **fixed** — grouped by name; 1.75x, and 20.8x on input that triggers it |
| Appending to a list you are growing, 14 input-scaling sites | **fixed** — an O(1) tail-append idiom; 1.15x here, and the quadratic removed from every input-sized loop |
| Discrimination index: per-insert cost is the node's fan-out | **fixed** — a symbol-keyed child index; 1.93x |
| Discrimination index: the key was a `short`, truncating above 32,767 symbols | **fixed** — `int32_t`; above the boundary the prover *aborted*, it did not degrade |

Cumulative, on `Problems/CSR/CSR040-10.p` (5.7 MB of TPTP, 44,227 clauses and
7,161 distinct symbols after translation), timing Prover9 **startup only**:
**168.17 s → 7.68 s**, a factor of **21.9**. What remains is 78% index
construction, which is still quadratic in the vocabulary and is the next piece
of work rather than a finished story.

#### Two lessons worth more than the speedups

**1. The expensive thing was not the inference. It was the setup work nobody
thinks of as an algorithm.** `predicate_elimination` is a preprocessing pass
that runs by default, and it was O(R³) in the number of relation symbols: it
eliminates one predicate per round and then recomputes its entire candidate
set, with each recomputation scanning every clause once per symbol. At 4,000
distinct symbols that was 29.57 s of startup against 1.27 s with the pass
switched off. Nothing about it is visible as a "search" cost; it happens before
the first given clause is selected. **Audit your own preprocessing and
index-construction passes with the same suspicion as its inference loop**, and
measure them against realistic vocabulary sizes rather than realistic query
sizes.

**2. An index sized by the vocabulary instead of by the data is the single
easiest way to make a KB engine quadratic.** Prover9's subsumption index keys
clauses on a *feature vector*: two entries for the clause, then two entries for
every relation symbol and four for every function symbol **in the whole
input** — and it builds that vector, as a linked list of one-integer cells,
once per clause. On the CSR problem above that is roughly 28,600 entries per
clause, 3.3 billion allocate/free cycles and 1.08 billion symbol-table lookups
for 37,537 clauses. The vector length is a property of the KB; the information
content is a property of the clause, which mentions perhaps five symbols. A
sparse representation would be O(clause size), but the index compares vectors
positionally and so expects a fixed length — which is why this is still only
half fixed here, and why it is worth getting right the first time.

**3. And the one that is easiest to write by accident: appending to a list you
are growing.** This is worth more attention than the other two, because it
produced **four of the five** performance fixes here from a single three-line
mistake repeated in different modules — the symbol multiset, the
predicate-elimination candidate set, the equational-definition dependents, and
the clause and symbol lists. The worst instance: on the CSR problem one symbol
collected 22,510 equational definitions whose dependent list reached 95,234
entries, so the accumulation cost **1.07 billion cell visits** — and every one
of those definitions was then discarded as cyclic.

The cause is the representation, not carelessness. In LADR a list *is* a
pointer to its first cell:

```c
typedef struct plist * Plist;
struct plist { void *v; Plist next; };
```

There is no list object, so `plist_append()` is handed only the head and has
nowhere to keep a tail pointer; the walk is forced. `plist_prepend()` is O(1)
for the same reason. The two look identical at a call site, and append is what
you reach for whenever order matters — which is most of the time. A scan found
**46** instances of `X = append(X, v)` inside a loop.

The lesson is about the data type, not the call sites: **give
your sequence type a tail, or a header that owns one, before anything
accumulates into it per fact.** Retrofitting is possible but awkward — here
`Plist` is passed by value across 92 call sites, so the fix had to be a
caller-held tail pointer and a documented helper rather than a change to the
type.

**4. A quadratic does not only cost time — it hides the next one.** The
clearest case: `arity_check()` compares every symbol name against every other
to report names used with two arities, which is almost always nothing. It was
239 of the 264 million string comparisons in one profile. When a demo for a
different fix happened to add two new constants per clause, this check became
the dominant cost and the demo reported *no improvement at all* for a change
that was really 2.9x. Four of the fixes here were found only because the one
before it had been removed first. Expect to re-profile after every change
rather than reasoning about what must now be fastest, and be suspicious of a
benchmark whose input shape you chose for convenience.

**5. A test suite built from small problems cannot see a change that only
matters at scale, or only matters to an invariant.** This one cost three
proofs. Giving the discrimination index a symbol-keyed lookup meant its child
lists no longer had to be sorted, and dropping that invariant required auditing
every reader of the structure. Two were missed — the function the delete path
uses to find a leaf still did a sorted walk, and the delete path never told the
index a node had been freed, leaving a dangling pointer. Three corpus problems
silently went from `THEOREM PROVED` to no proof.

The regression suite passed the whole time: 41 problems, 271 verified models, a
determinism check, upstream's own smoke tests, all green on a build that had
lost three proofs. What caught it was a 174-run A/B over a real corpus,
comparing normalised output against the previous build.

The transferable part is the *shape of the check*, not the bug:

- keep a corpus A/B against the previous build, not just a pass/fail suite.
  "Did any answer change?" is a different question from "do the tests pass",
  and only the first catches an invariant quietly breaking;
- compare only runs that stop for a reason the *input* determines — a proof, a
  model, exhaustion, a memory ceiling. A run stopped by a clock is cut wherever
  the clock landed, and comparing those produces phantom differences (this
  project lost a long detour to exactly that);
- establish the noise floor by running the **same** build against itself first.
  An A/B number is meaningless without it, and two of this project's early
  "nondeterminism" findings were the measurement, not the prover.

**6. Hand-rolled containers cost correctness, not speed.** Four accumulated
here — two hash tables, a grouping pass and a sorted-array merge — each 80-150
lines doing what a standard container does in one. Two are now
`std::unordered_map` and `std::vector` behind byte-identical C interfaces
(`extern "C"`, opaque pointers, one translation unit each), and the measured
speed difference is **1.04x**: essentially nothing. The case for doing it is
that the first version of one hand-written grouping contained a *fresh
quadratic* that only measurement caught, and the first version of the
discrimination index fixed the wrong half of the problem. 331 lines of probing,
tombstones, load factors and rehash deleted is the actual return.

The general form of all of these: **a structure or pass whose cost is
proportional to the size of the vocabulary, or to how much has already been
accumulated, executed once per fact.** That is fine at 300
symbols and fatal at 300,000, and it is invisible in any test suite built from
small problems. Prover9's own test suite is 41 problems of a few dozen clauses
each, and it passes identically before and after every one of these fixes.

**And LADR-2009 has no premise selection at all.** Nothing in the 2009 release
chooses a relevant subset of a large axiom set before the search begins; every
axiom enters the search space. LADR-2026 added premise selection, which is the
single most significant functional difference between the two releases for
large theories.

### The conclusion for a large-KB engine

A classical saturation prover handed a 500,000-formula KB does not fail
gracefully — it fails at the *parser*, then at the symbol table, then at
memory, long before the inference engine gets a chance to be the bottleneck.
Three implications for a large-KB engine:

1. **Relevance filtering is not an optimisation, it is the enabling
   mechanism.** Any architecture that reads the whole KB into the search space
   per query is bounded by KB size rather than query difficulty. Premise
   selection belongs in front of the reasoner, not inside it.
2. **The supporting data structures must be sized for the KB, not the
   query.** Every one of the super-linear behaviours above is a data structure
   chosen for a few hundred symbols (linear symbol-table scans, per-occurrence
   list insertion, recursion depth tied to input size). These are not clever
   algorithmic failures; they are defaults that were fine in 2005. Audit the
   equivalents in your engine against the real symbol and axiom counts. The
   two lessons above are the sharpest form of this: a *preprocessing pass* and
   an *index* whose cost is proportional to the vocabulary, run once per fact.

2a. **Measure before optimising, and measure on the shape you care about.**
   Every one of these was found by profiling, and none was where the previous
   step's reasoning said it would be. The symbol-multiset quadratic looked like
   the startup bottleneck and turned out to be 0.12% of it. `predicate_elim`
   is 106x faster on a synthetic ladder and 1.12x faster on the real CSR
   problem, because that problem's predicates are mostly not eliminable. A
   speedup measured on one input shape is a claim about that shape only.
3. **Interface limits are real limits.** A 255-arity ceiling and a fixed
   500 MB cap are not inference limitations, but they decide whether a problem
   can be attempted at all. Worth enumerating your engine's hard ceilings
   explicitly.

---

### A symbol's properties are not bounded by the input file

The single worst allocation in either program is sized by a property of a
symbol, and the symbols that made it catastrophic were not in the problem —
they were manufactured during preprocessing.

Mace4 tabulates every tuple of every symbol: arity *a* over a domain of size
*d* is `d^a` cells, allocated before any searching. TPTP
`Problems/LCL/LCL651+1.010.p` has a maximum arity of **2** of its own. It
clausifies to Skolem functions `f3` through `f29`, because Skolemising *n*
nested quantifiers yields a function of arity *n*, and those tables come to
`2^30` cells — an 80 GB request at domain size 2, before the first inference.
Nothing in the problem file hints at it.

That request was accumulated in an `int` (so arity ≥ 31 wrapped silently),
never checked for failure (so a memory limit turned it into an immediate
`SIGSEGV` rather than a diagnostic), and allocated outside the accounted
allocator (so the program's own memory ceiling could not see its largest
allocation, and reported figures that omitted it). Those three together cost a
machine reboot and 6% of a sweep's runs.

**This turned out to be four defects, not one.** Bounding the allocation the
crash pointed at left the identical fault in three other places, each found
only after the previous was fixed: a second per-symbol structure growing
`(d+1)^arity` instead of `d^arity` and enabled by default; a library
allocator that bypassed accounting for every block over 4 KB, so the reported
ceiling was holding about twice what it claimed; and a limit that was switched
off entirely until the first node of the search, leaving everything the engine
did to *prepare* for searching unbounded. The last of those is the one that
could take a machine down, and it was the one no crash pointed at.

So the first lesson is about method: when the finding is "this program has
allocations its accounting cannot see", enumerate **all** of them before
fixing any. A `grep` for `malloc|calloc|realloc` is a two-minute job and it
was in hand, in writing, when the first fix was made.

**For a KB engine** the transferable part is not "watch out for Skolem
functions". It is that any per-symbol structure sized by a symbol property —
arity, number of arguments, domain cardinality, a type's extension size — will
eventually be handed symbols that no human wrote, with properties no input
file shows: from clausification, from rule compilation, from reification, from
a query rewriter. Three guards cover it, and none of them needs to know where
the symbol came from:

1. **Compute the size in a type that cannot wrap**, and check it against what
   the index can actually address. Here the cell *count* had to fit an `int`
   because ids are `int`, which is a lower limit than memory — a size can be
   affordable and still unusable.
2. **Check the allocation.** An unchecked `malloc` under a memory limit is a
   crash where a diagnostic belongs, and it crashes *before* doing any work,
   so it looks like a bug in the input rather than a resource limit.
3. **Charge it to the budget the engine reports.** A ceiling that cannot see
   the largest allocation is worse than no ceiling, because it is believed. If
   some allocations bypass the accounted allocator, either route them through
   it or add them into the figure the limit is compared against.

4. **Check where the work happens, not where it is convenient to check.**
   Mace4's memory check ran at the top of its search recursion, which is the
   obvious place and reads as thorough — every node, no exceptions. But the
   allocation that exhausted the machine happened in *preprocessing*, before
   the first node, where the check could not run at all. A limit enforced on
   one phase of a multi-phase engine is not a limit. For an inference engine
   this means the rule-compilation, indexing and instantiation phases need the
   same ceiling as the inference loop, and a resource check belongs in the
   allocator, where every phase has to pass through it, rather than in the
   loop you happen to be thinking about.

Also worth copying: the refusal stops the search rather than trying the next
domain size, because a larger domain cannot need fewer cells. When a resource
check fails, work out whether the next step is *necessarily* worse; if it is,
stopping with a clear reason beats discovering it again more expensively.

And make the out-of-memory path re-entrant-safe. Mace4's new handler is called
from inside the allocator, so its own diagnostic allocated, re-entered the
handler, and recursed until the stack was gone — turning a clean refusal into
a segfault. Disarm the check, then report; and if re-entered anyway, leave
without touching the allocator.

## 10. Defects, and what they say about trusting a translation layer

The audit found and fixed a number of defects in LADR-2009-11A; the full
write-up is in `FIXES.md`. Two bear directly on anyone using these tools to
generate a comparison baseline, and one of them produces **wrong answers**.

### The dropped negation (wrong answers)

TPTP permits `~` to apply to a quantified formula without parentheses.
`tptp_to_ladr` translated such a formula into its **opposite**, silently:

```
TPTP in:   fof(ax4, axiom, ~ ! [U] : (sorti2(U) => ...commutative...) ).
LADR out:         sorti2(U) -> (all V (sorti2(V) -> op2(U,V) = op2(V,U)))
```

The input says the operation is *not* commutative. The output asserts that it
*is*. Measured across the forms that matter:

| TPTP | should be | was emitted |
| --- | --- | --- |
| `~ ! [X] : p(X)` | `-(all X p(X))` | `p(X)` |
| `~ ? [X] : p(X)` | `-(exists X p(X))` | `p(X)` |
| `~ ! [X] : ~ p(X)` | `-(all X -p(X))` | `-p(X)` |
| `! [X] : ~ ? [Y] : q(X,Y)` | `(all X -(exists Y q(X,Y)))` | `(all X q(X,Y))` |
| `~ ( ! [X] : p(X) )` | `-(all X p(X))` | correct — the parentheses saved it |

**1,930 of the 17,912** first-order problems contain the shape.

Mechanism: `~` is declared at precedence 410 and `:` at 450, so the parser
binds `~` to the quantifier *prefix*, producing `:(~(!([X])), p(X))` instead of
`~(:(!([X]), p(X)))`. The converter's `:` branch — carrying the comment
`/* Assume it's well formed (don't check for errors). */` — then called
`listterm_reverse()` on `!([X])`, which is not a list, received the empty list,
never entered its variable loop, and returned the body. Negation and quantifier
both vanished. The same branch also chose `exists` for any quantifier that was
not `!`, without checking.

Precedence alone cannot fix it. Raising `~` above `:` makes
`~ ! [X] : p(X)` parse correctly and immediately breaks `! [X] : ~ p(X)`,
because `:` can then no longer take a `~`-formula as its right operand; the
failures swap and the count stays at four. Setting them equal breaks the parser
outright. TPTP's grammar permits both nestings because `~ <unitary>` and
`! [X] : <unitary>` recurse through the same nonterminal, which one precedence
per operator cannot express. McCune's own Changelog records two attempts, one
noted as *"The Feb 14 precedence change (~) was no good"*, and the line still
carries his `/* changed by McCune */` comment. The repair is in the converter:
peel any `~` bound to the quantifier prefix, build the quantified formula, then
re-apply the negations outside it.

**How it surfaced is the part worth passing on.** The capability sweep flagged
dozens of problems where Mace4 reported a model for a problem TPTP declares a
`Theorem` — which would have been a soundness bug in the model finder, a very
large claim. It was not. The translator had inverted an axiom and Mace4 was
correctly finding a model of a different problem. On `ALG029+1`, the stock
build finds a model; with the negation preserved it finds none, as it must.
Every apparent soundness violation traced back to the translator, and none to
Prover9 or Mace4.

### The unreadable labels

A TPTP formula name may be a single-quoted atom. TPTP quotes with `'` and LADR
with `"`, and the name was copied verbatim into `label(...)`:

```
fof('EQ-var', axiom, ...)   ->   ... # label('EQ-var') # label(axiom).
```

LADR's own reader then rejects the file the translator just wrote, with
`Fatal error: sread_term error` pointing at the label. **382** problems carry
names of this form, and every one failed at the *prover* rather than the
translator, so nothing in the translation output hinted at the cause. The `-q`
option does not help: it remaps function and predicate symbols, not names.

### One more worth knowing about

`include('f.ax', [names])` — TPTP's *selective* include — is accepted by
`tptp_to_ladr` and then **ignored**: the whole file is included. That can
silently *add* axioms rather than fail, which is a wrong-verdict risk rather
than a parse failure. Only two problems in this sweep use the form, but the
failure mode is the dangerous kind. (Found by a sub-agent during this audit;
not fixed.)

Separately, `set(production)` mode — the forward-chaining mode most relevant to
a chaining engine — contained a heap buffer overflow on **every** run, in
`ladr/dollar.c`, where an array was sized from a symbol count and then filled
using calls that increase that count. It is present in stock LADR-2009-11A and,
independently confirmed, in LADR-2026. Fixed here.

### The general lesson

All of these live in the **translation layer between a knowledge
representation and a reasoner**, not in the reasoner. The dropped negation is
the clearest case: the prover and model finder were both behaving correctly and
the pipeline still produced a wrong answer, with no diagnostic anywhere.

If an engine converts its KB into a reasoner's input language — its own or an
external prover's — that converter deserves the same scrutiny as the inference
engine, and specifically:

- **Round-trip the translation.** Translate, translate back, compare. Our
  sub-agent's preprocessing work used a byte-exact inverse round trip as its
  correctness criterion, which catches a dropped connective immediately.
- **Count formulae across the boundary.** A translation that silently drops or
  duplicates a formula is caught by comparing counts against the source
  header.
- **Fail loudly on anything unrecognised.** The `/* don't check for errors */`
  comment is the root cause of the worst defect here. A converter that cannot
  represent its input must say so, not emit its best guess.
- **Use a labelled corpus as the end-to-end check.** The TPTP `Status` field
  caught this; no amount of output-diffing would have. Any engine has the same
  opportunity with these 17,073 problems.

---

## 11. Retrieval order is a semantic parameter, not an implementation detail

This is the most transferable thing the project learned, and it cost a
regression to learn.

A discrimination tree indexes terms so a query can find the stored terms that
match it. This tree's nodes kept their children in a sorted linked list, and
both insertion and lookup walked it, so each cost the node's fan-out. On a
knowledge-base-scale problem that was 105.9 million sibling steps in one run,
91% of them at depths 1 and 2. Replacing the walk with a per-node index made
startup 1.93x faster, and making insertion O(1) required dropping the sort
order — which was argued to be safe, because **retrieval never walks the rigid
children; it looks the symbol up directly.**

That argument was true and the conclusion was wrong. Retrieval does not walk
the children, but the child order **is** the order of the depth-first walk over
the tree, and therefore the order in which the index hands back
equally-matching answers. Demodulation and forward subsumption each take the
**first** answer. So reordering the children changed which rewrite rule was
applied, which changed the normal form, after which two clauses that would have
subsumed each other no longer looked alike. Same problem, same input, same 47
given clauses:

```
  ordered children     Generated=723      Kept=521      Forward_subsumed=201  proof in 0.03 s
  unordered children   Generated=1052629  Kept=1052475  Forward_subsumed=154  2 GB exhausted after 79 s
```

Twelve TPTP problems stopped being answered. Nothing in the test suite saw it.

### What to take from it

**If your engine has an index over rules or assertions, the order it returns
candidates in is part of your inference strategy.** Any place that takes the
*first* match — a rewrite rule, a subsumption check, a "most specific rule
wins" conflict resolution, a cached first-answer — is reading that order as
semantics. For a KB engine this is likely to apply wherever a rule index is
consulted and the engine commits to one candidate rather than enumerating all
of them.

Three concrete practices:

- **Write the order down as an invariant, in the data structure.** The hash
  table here was replaced with a sorted array, not because the hash was wrong —
  it was instrumented against an order-independent scan over 4 million lookups
  with zero divergences — but because **an unordered container is the wrong
  place to keep order-critical data.** A sorted array has nowhere to put an
  out-of-order element; a comment above a hash table has nothing to stop the
  next change. It also turned out to cost nothing: keys stored inline in a
  parallel integer array make the binary search cache-friendly enough to match
  the hash (5.0 s against 5.1 s on the same benchmark), while deleting ~150
  lines of probing, tombstones and rehash.
- **Test the invariant, not just the lookup.** The existing unit tests asked
  what the index *returned* for a symbol — always correct — and never what
  order the children were in. The new test inserts 400 symbols in a fixed
  scramble and asserts the ordering, then deletes every third one and asserts
  it again. It fails against the unfixed code. **A test that does not fail on
  the defect it is named for is not evidence.**
- **Expect the symptom to point somewhere else.** The visible failure was
  "forward subsumption stopped working", in a file that had not been touched.
  The cause was two subsystems away. A `git bisect` with a one-line pass/fail
  predicate found it in seven builds and about four minutes; reading the
  subsumption code would not have.

## 12. How to tell a capability change from a correctness change

When a pipeline has more than one stage and you change two of them, a
difference in results cannot be attributed by looking at the results.

This tree fixed both the translator and the prover. Measured against the
pristine release over 8,996 TPTP problems, prover9's answers fell from 2,353 to
2,219 — which reads as a 134-problem regression and was quoted that way in this
project's own notes before it was checked. It was not one, because **a
corrected translation is a different problem**, usually a harder one: the old
build was answering a formula the input never contained, and losing that is the
fix working.

The method that separates them is a **cross-feed**: run every combination of
{old, new} stage-1 output x {old, new} stage-2.

| | old prover | new prover |
| --- | ---: | ---: |
| **old translation** | 205 answers | 190 |
| **new translation** | 18 | 6 |

Read down a column and the input explains almost all of it; read across a row
and the prover explains twelve. Per problem: **187 input, 12 prover, 6 noise.**
The twelve were the regression in section 11.

The shape recurs whenever the KB loader, the canonicaliser or
the rule compiler changes alongside the engine. The cheap version is to keep
every stage's output addressable so an old stage can be fed to a new one.

### And a soundness oracle that needs no reasoning

The strongest check in this whole project required no prover at all. **Every
TPTP `fof` formula is closed, so a free variable in the translated output is
wrong on its face.** One 20-line script over 275 problems: the old translator
emitted 1,146 formulas with a free variable, the fixed one emitted 0. No
equivalence checking, no oracle labels, no judgement — and it localises the
defect to the translation layer immediately.

The general move is to find invariants of your pipeline that are checkable
without doing the reasoning: closed in, closed out; arity preserved;
variable count preserved; symbol set preserved modulo Skolem functions;
formula count matching the source header. Each one is cheap and each one rules
out a class of silent corruption. This is worth more per line than any amount
of output diffing.

## 13. What the gates could and could not see

Worth recording as a taxonomy, because it generalises to any engine with a test
suite.

With the section 11 defect in the tree, **all of these were green**: 8 unit
test binaries, 41 regression tests, 271 verified models, a 6-case demonstration
suite, 26 translation-semantics checks, a determinism harness, a 174-problem
behavioural A/B against the previous build, ASan+UBSan over 336 prover/problem
pairs, a crash-poison sweep over 336, and valgrind memcheck over 168.

Every one of those asks the same question: **does this build still do what the
last build did, on inputs we already have?** The defect was a loss of
capability on problems none of those sets contained, so none of them could
answer it. What found it was the one instrument that asks a different question
— *can we still answer what the baseline could answer* — run against the
original 2009 release over a large external problem set.

The practical conclusion, now wired into this tree as `make cap`: keep a
**capability gate** separate from the regression gate, and populate it not with
a random sample but with the problems that have **ever changed hands** between
two versions. Those are enriched for sensitivity — near a limit, order-
dependent, formerly crashing — so a few hundred of them catch in minutes what
an 8,996-problem sweep catches in an hour. The equivalent for a KB engine is a
standing set of queries that have ever flipped between answered and unanswered,
checked on every engine change.

**Point it at the version you are replacing, not at the original.** The same
instrument, same problem list, answers two different questions depending on
the baseline, and only one of them is a verdict. Against the previous build a
loss is a regression: this tree's gate reports 0 lost, 0 gained, with a noise
floor of zero measured by giving it the same build on both sides. Against the
2009 release it reports 184 lost and 75 gained, which is not a failure but
section 5's differential restated — a corrected translation is usually harder
than the wrong problem it replaces, and section 12's cross-feed is what splits
those 184 into 187 input, 12 prover, 6 noise. Running it against the original
and reading the loss count as a verdict is a mistake that is easy to make and
costs an hour; this tree made it, which is why the target now names the
baseline it wants.

One more, which is about process rather than testing. The commit that
introduced the section 11 defect **predicted it in its own message** — "making
indexed children unsorted changes which answer is returned first, so it can
change which demodulator is applied" — and said the comparison "is NOT yet run
and is required before it reaches rc". It was merged without it. **A
precondition recorded in a commit message is not a gate.** If it must happen,
it has to be a target something runs.

## 14. Validate the instrument on a case whose answer you know

Two measurement errors in this project were worse than the defects they were
looking for, because a wrong measurement sends the work in a wrong direction
and looks like data while it does it.

- A wall-clock guard on prover runs turned CPU contention into spurious
  timeouts and **faked 38.9% of one sweep's timeouts**. Fixed by bounding CPU
  time with `ulimit -t`.
- A peak-memory script sampled `/proc/<pid>/status` for every process matching
  `pgrep -x mace4` — i.e. for whatever else on the machine was called `mace4`.
  Run while a 64-job sweep was going, it reported **1,465 MB of peak memory for
  the input `p(a).`**, and it briefly made a working fix look ineffective.
  Fixed by using `getrusage` through `/usr/bin/time`, which measures the child
  and nothing else.

Both were caught the same way: **run the instrument on an input whose answer
you already know.** A trivial input must measure trivially. It is one extra
line in any benchmark harness and it would have caught both immediately.

Related and already recorded in section 9: CPU time is **not** load-invariant.
The same binary on the same input measured 20.48, 18.95, 10.87 and 10.14 CPU
seconds across four interleaved runs as machine load varied — a 2x spread with
no code difference. Any A/B at a fixed time budget must interleave the two
builds in one queue, and treat a result within about a third of the limit as
not reproducible.

## 15. What the licence reaches, and what it does not

These are points of fact about what the licence notices in this tree say, read
out of `COPYING` and the per-file notices. They are not legal advice. If it
matters commercially, ask a lawyer rather than taking a maintenance tree's
notes at face value.

**Start with the case where none of it applies.** Copyright protects
expression, not ideas: 17 U.S.C. §102(b) excludes "any idea, procedure,
process, system, method of operation, concept, principle, or discovery" from
protection, however it is described or illustrated in the work. Sections 1
through 14 above are ideas, techniques and know-how — the given-clause loop
and its option names, hints, the set-of-support discipline, the indexing and
retrieval-order findings, the scale limits, the measurement and gating methods
— and reading them here and reimplementing them in another engine carries no
obligation from LADR's licence whatsoever. **That is the intended use of this
document**, and it is why the sections are written as findings with the source
lines cited rather than as code to lift.

The rest of this section applies from the moment **code** crosses over: a
vendored source file, a linked `libladr.a`, a copied function, a patched
binary shipped onward. Those are copies of expression and they carry their
terms with them.

**LADR is GPL-2.0-only.** 250 of the release's 307 `.c` and `.h` files carry
the same notice byte-for-byte — every compiled one — and it says "version 2",
not "version 2 or any later version". There is no upgrade clause, so this code
cannot be redistributed under GPL-3 and cannot be combined with GPL-3-only
code. Anything built from it inherits that, and the inheritance is not
negotiable: the only grant received is the GPL-2 grant. Check this before
planning to link a reasoner against it alongside anything GPL-3, Apache-2.0 or
proprietary, because the answer is usually no, and a process boundary with the
prover reading a file is what people use instead.

**A performance change can be a licensing change.** This tree added one C++
translation unit to `libladr.a`, so every link line that touches libladr now
carries `-lstdc++` — a library the 2009 release never linked (`git grep
stdc++ 25b4505` returns nothing). libstdc++ is GPL-3, and what makes it
legitimate inside a GPL-2-only program is the GCC Runtime Library Exception,
which permits exactly this provided the whole thing is compiled by GCC without
a proprietary plugin. Fine here; it would need re-examining under a toolchain
where that exception does not apply. The general point is that **the link line
is part of the licence surface**, and a change made purely for speed can move
it without anyone noticing.

**Absence of a notice is not absence of a licence, but it is a decision
someone has to make.** Twenty files in the release carry no licence text at
all: the seven Makefiles, `VERSION_DATE.h`, five dead sources under
`apps.src/old/`, four Python scripts and three scripts under `utilities/`.
Nothing about them is unlicensed — they are plainly parts of the same release
— but a scanner that keys on notices will flag them as unknown and a reviewer
will ask. They are inventoried in `LICENSING.md`, and each now carries the
release's own notice with a line saying the notice was added here and that
McCune did not write it there. If you vendor any third-party tree, do that
inventory once and write down both halves: which files carry what, and what
you concluded about the ones that carry nothing.

**GPL-2 §2(a) wants a per-file change notice**, "stating that you changed the
files and the date of any change". Ninety-one files here carry one, directly
beneath the upstream notice and never touching it. The part worth copying is
that nothing in it is typed by hand: each file's dates are the first and last
of `git log --format=%ad --date=short 25b4505..HEAD -- <path>`, and the notice
ends with the command that prints that file's own diff, so a reader checks the
claim instead of believing it. A notice you can regenerate from the history is
a notice that stays true after the next change.

## 16. Keep the unmodified drop as a commit, and make the measurement an artefact

Two provenance habits paid for themselves repeatedly here, and neither costs
anything if done first.

**The first commit is the untouched 2009 tarball.** That one decision is why
most claims in this document can end in a command rather than an assurance:
each per-file notice names `git diff 25b4505 -- <that file>`, the licence
inventory above was counted rather than recalled, and the cross-feed in
section 12 was possible at all because the superseded stage could still be
rebuilt and run. The equivalent is to commit the unmodified KB
drop before the loader, the canonicaliser or the rule compiler touches it, and
to keep each stage's output addressable. **You cannot cross-feed stages you
cannot reconstruct**, and by the time you want to it is too late to start.

**A result that lives at a path on one machine is not a result.** The
capability numbers in sections 8 and 9 were produced by a harness this
repository's README pointed at as `~/Projects/TPTP/results-prover9mace4/` — a
directory in neither published repository, which no reader could fetch and
which nothing would have caught, because the sentence was true on the machine
where it was written. The fix was to make the corpus a distributable thing in
its own right: the harness, every scored sweep, its own copy of the licence
because it is now distributed separately, and a clone line in both READMEs.
The rule: if a number appears in a document someone else will read, whatever
produced it has to be fetchable by them, and the pointer has to be checked
from outside your own home directory.

## 17. Comment-only edits are program changes until a gate says otherwise

Putting licence headers across the tree looked like the safest commit in the
project. It was not, for one reason: **a theorem prover echoes its input.**
The eight prover inputs under `test.src/demos/` are read by prover9 and mace4,
which reproduce what they read in their output, so a twenty-line licence block
in each would be twenty lines inside every expected-output comparison and
twenty lines of noise in front of the thing each demonstration exists to show.
They carry a three-line notice instead, and the whole gate — build, eight unit
binaries, 41 regression tests, 26 translation-semantics checks, 271 verified
models, the six demonstrations and the baseline discrimination check — was
re-run on the comments-only commit. That re-run is the only reason
"comments-only" is a statement of fact here rather than an assumption.

Two checks from the same exercise are worth stealing, both cheap:

- **Verify that the untouched thing is untouched.** After editing 80 source
  files, the upstream licence block in each was hashed and compared against
  `25b4505`: 80 of 80 identical. "I did not alter his notices" and "no byte of
  them changed" are different claims, and only one of them is checkable.
- **Ask what downstream depends on line numbers.** Here, `__LINE__` occurs in
  exactly two places — the `SYMNUM` and `VARNUM` checked accessors in
  `ladr/term.h`, which print only on a fatal type violation — and no
  expected-output file cites a source line, so inserting ten lines at the top
  of a file was safe. In a system that logs `file:line`, hashes its own
  source, or ships a symbolised trace inside a test fixture, it would not have
  been.

The generalisation for a KB pipeline, where most of what moves is text that
gets echoed, logged and diffed: **"non-semantic" is a property of the
pipeline, not of the edit.** A comment is semantic in any stage that copies
its input to its output — which includes almost every stage that reports what
it did.

---

## Appendix: what was and was not tested

Tested and measured on this machine: everything with a number attached above,
and the behaviour of every option quoted from the source has been read in the
source at the cited line.

Not tested: LADR-2026's capability on this corpus (its defect comparison was
researched but not independently rebuilt and run); whether the TPTP ratings
from v4.0.x were computed over a prover set that included Prover9 itself, which
would bias the era-relative figures; and the preprocessing shim that would
rescue a further 274 translation failures, which was verified for content
preservation but is not part of the reported run.

---

Copyright (C) 2026 Chris Arena, 2026-10-16 through 2026-10-17.  Written in
collaboration with Claude Opus 5 (Anthropic).  GPL-2.0-only; see `COPYING`.
Not part of the LADR-2009-11A release.
