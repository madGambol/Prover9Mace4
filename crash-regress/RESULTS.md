# Prover9Mace4 crash reproducers vs. LADR-2026 (AlgorithmicTruth/Prover9 @ 62ae6ff)

Run 2026-10-27. Every reproducer that FIXES.md, `test.src/regress.sh`,
`test.src/mace4_cells_test.sh`, `test.src/demos/` and the unit tests in
this tree give for a user-triggerable crash, abort, hang,
sanitizer error or wrong answer, run against:

| label | what | how built |
| --- | --- | --- |
| `baseline-2009` | LADR-2009-11A + build fixes only (commit `04567a4`): the "crash expected" control | `git archive 04567a4` -> `build/base`, `make` |
| `old-fixed` | this tree's fixed build, `../bin` (used as is) | -- |
| `new-release` | LADR-2026, a clone of AlgorithmicTruth/Prover9 at `62ae6ff`, built with `make all -j8` (v2026-8A) | -- |
| `*-asan` | the three trees with `-fsanitize=address,undefined,float-cast-overflow -g -O1` | `build/{base,old,new}-asan`; 137/137, 139/139, 144/144 compile lines carry `-fsanitize`; 77/77, 79/79, 81/81 `libladr.a` members reference `__asan` |
| `*-poison` | the three trees linked with `-Wl,--wrap=free_mem wrap/poison_wrap.o` | `build/{base,old,new}-poison` (see M8) |

Files: `run_all.sh BIN_DIR [LABEL] [CASE...]` re-runs everything; `make_cases.sh`
regenerates `cases/`; `NEW_TREE=<clone> build/build.sh` rebuilds the control/instrumented trees;
`tabulate.py logs` prints the comparison; per-case `cmd/out/err` and
`summary.tsv` are in `logs/<label>/`; one-off evidence is in `logs/extra/`.

## Result table (one row per case)

Verdicts: **pass**, **abort** (SIGABRT), **crash** (SIGSEGV), **hang**
(timeout), **wrong** (wrong answer/output), **fatal** (clean error where the
input is valid or the option legal), **sanitizer** (ASan/UBSan report),
**invisible** (bug class not observable in that build type).

| id | symptom (FIXES.md) | reproducer | 2009 baseline | old fixed | **new 2026** | note |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | stack overflow, `char str[20]` banner | `mace4 -n 10000000 -f tiny.in` | abort (fortify) | pass | **pass** | buffer now `char[24]`; then "too many cells" refusal |
| M2 | stack overflow, missing-file msg | `prover9 -f <300 z>` | abort | pass | **abort** (fortify) | `ladr/top_input.c:965-966` unchanged |
| M2b | same, via mace4 | `mace4 -f <300 z>` | abort | pass | **abort** | same site |
| M3a | same, prooftrans | `prooftrans -f <300 z>` | abort | pass | **abort** | `apps.src/prooftrans.c:382-383` |
| M3b | same, directproof | `directproof -f <300 z>` | abort | pass | **abort** | `apps.src/directproof.c:668-669` |
| M3c | same, interpformat | `interpformat -f <300 z>` | abort | pass | **abort** | `apps.src/interpformat.c:217-218` |
| T1 | same, from a TPTP `include()` | `tptp_to_ladr < longinc.p` | abort | pass | **pass** | now `snprintf(... "%.60s" ...)` |
| M5 | stack overflow, `-label` | `prooftrans hints -label <300> < proof.txt` | abort | pass | **abort** | `prooftrans.c:575-578` unchanged |
| F1 | input used as printf format | `unfast < fmt.in` | crash | pass | **pass** | `fastparse.c:186` now `"%s"` |
| D1a | infinite loop on truncated proof | `prooftrans < trunc.txt` | hang | pass | **crash** after 26 s | loops forever growing memory (~95 MB/s); dies when the 4 GB cap is hit |
| D1b | same | `directproof < trunc.txt` | hang | pass | **crash** after 26 s | same |
| D1c | control: complete proof | `prooftrans parents_only < proof.txt` | pass | pass | pass | |
| D1d | D1 side effect: extra blank line | `prooftrans < proof.txt` | wrong | pass | **wrong** | cosmetic; same root cause (a) |
| D2a | `fscanf("%s")` into `char[1000]` | `prooftrans < longtok.txt` (200,000-char id) | crash | pass | **crash** | `prooftrans.c:456,472,499` |
| D2b | same | `directproof < longtok.txt` | crash | pass | **crash** | `directproof.c:704,720,737` |
| T2a | long name truncated in msg (usability) | `tptp_to_ladr < inc4000.p` | abort | pass | **fatal**, name cut at 60 chars | safe, not a crash |
| T2b | same, prover9 | `prover9 -f <4000 y>` | abort | pass | **abort** | this is M2 at 4000 chars |
| T3 | circular `include()` recursion | `tptp_to_ladr < loop.p` | crash | crash | **pass** | new has an include-depth limit of 20: clean fatal error. FIXES' T3 only deepened the recursion, so old-fixed still crashed at the time of this run; the same depth limit was ported to this tree afterwards (FIXES.md T3, amended 2026-10-27), and old-fixed now passes |
| TR | control: translate + prove | `tptp_to_ladr < grp.p \| prover9` | pass | pass | pass | |
| T4a | `~ ! [X] : p(X)` loses the negation | `tptp_to_ladr < neg.p` | wrong | pass | **wrong** (emits `p(X)`) | `ladr/tptp_trans.c:377-401`, identical to 2009 |
| T4b | same, semantic | `neg2.p` must prove | wrong | pass | **wrong** | |
| T4c | nested `! [X] : ~ ? [Y]` | `negnest.p` must prove | wrong | pass | **wrong** | |
| T4d | unsound direction | `neginv.p`: `p(c)` must NOT prove | wrong (proves) | pass | **wrong (proves)** | the translator asserts the opposite of the axiom |
| T5 | quoted formula name unreadable | `qn.p` | fatal | pass | **fatal** (sread_term) | |
| T6a | distinct objects not lexable | `do.p` must prove | fatal | pass | **fatal** | |
| T6b | distinct obj may equal term | `dc.p` must NOT prove | fatal | pass | **fatal** | |
| T7a | `include(f,[names])` ignores the list | `sel.p` must omit i2 | wrong | pass | **wrong** | `provers.src/tptp_to_ladr.c:38` |
| T7b | unsound direction | `selq.p`: q(b) must NOT prove | wrong (proves) | pass | **wrong (proves)** | |
| T7c | control: `include/1` | `all.p` | pass | pass | pass | |
| T8a | `~&` not declared | `nand.p` | fatal | pass | **fatal** | |
| T8b | `~\|` not declared | `nor.p` | fatal | pass | **fatal** | |
| AR | MAX_ARITY 255 (demos/arity-ceiling) | `a300.p` | fatal | pass | **pass** | `MAX_ARITY USHRT_MAX` |
| T4n..ARn | same TPTP inputs through new `prover9 file.p` (own reader) | `prover9 neg2.p` etc. | n/a | n/a | **pass** x9, **wrong** x1 | native reader is right on T4/T5/T6/T8/arity; **T7bn wrong**: the native reader also ignores the include name list, and its proof uses the excluded `i2` |
| M9a | cell table: int total wraps | `mace4 -f wide.in` (p/40) | wrong (SIGSEGV rc 102 after reaching the 16 GB cap) | pass | **pass** | new `int_power` check refuses it: "too many cells" |
| M9b | cell table ignores max_megs | `big.in` (q/29, max_megs 500) | wrong (SIGSEGV) | pass | **fatal** | requests 30,064,771,296 bytes despite max_megs 500. Fails cleanly here only because of my 16 GB cap; uncapped it would allocate it |
| M10 | negprop index ignores max_megs | `np.in` (r/15, max_megs 20) | wrong (467 MB RSS) | pass | **wrong** (467 MB RSS) | |
| M12 | max_megs off before search | `mace4 -n 10 -N 10 -f ground.in` (max_megs 20) | fatal (OOM at cap) | pass (26 MB) | **wrong** (15.9 GB RSS, ran to max_seconds) | `msearch.c:1525 disable_max_megs()` |
| M12c | control: a feasible model | `ok.in` | pass | pass | pass | |
| M11 | max_megs counts ~half of memory | `prover9 -f megs500.in alg033.in` (TPTP ALG033+1) | wrong: 548 MB RSS, 582 MB uncounted | pass: 244 MB | **hang** at 120 s, in CNF (see below). One 300 s run: **wrong**, 553 MB RSS, palloc 500 + 577 MB uncounted | `ladr/memory.c:315` |
| N1a/b/c | `max_minutes/hours/days,-1` abort | `prover9 -f n1_*.opt.in triv.in` | fatal | pass | **fatal** x3 | `ladr/options.c:850` unchanged |
| N2a/b | multiplier overflow | `max_minutes 50000000`, `max_hours 1000000` | fatal (+UBSan) | pass | **fatal** x2 (+UBSan overflow) | derived max_seconds -1294967296 / -694967296 |
| N3 | `pick_given_ratio,-1` aborts | `n3_pgr.opt.in` | fatal | pass | **fatal** | |
| Nc | control: in-range derivations | `max_minutes 2`, `pick_given_ratio 4` | pass | pass | pass | |
| U1 | UBSan: overflow every default run | `prover9 -f x2.in`, UBSan build | sanitizer | pass | **pass** | fixed differently (`giv_select.c:181-191`) |
| M7 | heap overflow, `set(production)` | `prover9 -f prod.in`, ASan build | sanitizer (plain 2009: **abort**, glibc heap assert) | pass | **sanitizer** (ASan heap-buffer-overflow `dollar.c:75`); plain: invisible | valgrind on the shipped new binary: 6 invalid writes after the 320-byte block |
| M7b | same, demo input | `m7-production-overflow.in` | sanitizer | pass | **sanitizer** | |
| M8 | UAF in `zap_flatterm` | `prover9 -f x2.in`, poisoned build | crash | pass | **crash** (SIGSEGV at `flatterm.c:152`, gdb) | same call chain as FIXES.md |
| T9 | discrim key in a `short` | unit `discrim_test_t9.c` (40,000 symbols) | wrong (round-trip FAIL) | pass | **pass** | `discrim.h:89` is now `int symbol` |
| PF5 | 10 recursive list functions SEGV at 400k | unit `pf5_lists.c` | wrong (crashed) | pass | **pass** | all ten are loops in 2026 |
| M13 | `int_power` wraps | unit `intpower_test.c` | wrong (+UBSan) | pass | **pass** | rewritten with a pre-division check |
| X1-5 | helper scripts unrunnable | shebang/exec bit + `looper`, `attack`, `gvizify`, `prover9-mace4` | wrong (rc 127) | pass | **wrong** (rc 127) | `#!/usr/bin/python`, py2 `print`, gvizify mode 644 |

## Counts for the new build, by fix id

Runtime-testable ids: M1 M2 M3 M5 F1 D1 D2 T1 T2 T3 T4 T5 T6 T7 T8 ARITY M7
M8 M9 M10 M11 M12 M13 N1 N2 N3 U1 U2 U3 T9 PF5 X1-5 (32).

- **Still crash or misbehave: 20.**
  - Crashes and memory errors (7): M2, M3, M5, D1, D2, M7, M8.
  - Wrong answers or lost problems in `tptp_to_ladr` (5): T4, T5, T6, T7, T8.
    T7 is also wrong in the native `prover9 file.p` reader.
  - Options and limits (7): N1, N2, N3, M9 (the max_megs half; the int-wrap
    half is fixed), M10, M11, M12.
  - Tooling (1): X1-X5.
- **Usability only (1):** T2. Messages are truncated at 60 characters in
  `tptp_to_ladr`; on the M2 path it is the M2 abort.
- **Fixed in the new build (9):** M1, F1, T1, T3, ARITY, M13, T9, PF5, U1.
  Several were fixed differently from the old project: M1 by a 24-byte buffer
  rather than snprintf, T3 by an include-depth limit, U1 by an explicit
  overflow check rather than a saturating helper.
- **Not tested dynamically (2):** U2 and U3. FIXES.md gives no reproducer;
  it says they need a search that reaches the `sos_limit` path with an
  `INT_MAX` part populated. Two probes (`cases/u2_probe.in`,
  `cases/u3_probe.in`) did not trigger them even on the 2009 UBSan build, so
  they prove nothing. By inspection, both are still present:
  `giv_select.c:113 sum += gs->part` is unsaturated, and `:561 int iters` is
  still assigned a double. One more finding: `least_iters_to_selection`
  starts at `INT_MAX`, so U3's conversion can only overflow through U2's
  wrapped-negative cycle size.
- M11 needed one run past the 30-120 s bound. At 120 s the new prover9 never
  gets out of clausification (see the incidental finding below), so I ran it
  once with `M11_TIMEOUT=300`. It is single-threaded and peaked at 553 MB.
  Those logs are in `logs/extra/M11_new-release_300s.*`. `run_all.sh`
  defaults to 120 s.

## What still fails in 2026, where, and whether the old fix ports

All paths are relative to the LADR-2026 clone (AlgorithmicTruth/Prover9 at `62ae6ff`). "Ports
directly" means the 2026 function is byte-identical (or nearly so) to the 2009
one the old patch was written against. I measured this by diffing the
function bodies of `build/base` against the 2026 tree.

- **M2/M3, T2b.** `ladr/top_input.c:965-966`, `apps.src/prooftrans.c:382-383`,
  `apps.src/directproof.c:668-669` and `apps.src/interpformat.c:217-218` still
  do `char s[100]; sprintf(s, "...file %s not found", argv[...])`.
  `read_all_input` is identical to 2009. **Ports directly.** Either use the
  old project's `fatal_file_not_found()` (`ladr/fatal.c`, which measures with
  `snprintf(NULL,0)` and mallocs), or at minimum a larger buffer plus
  `snprintf`, as `tptp_to_ladr.c:53` already does.
- **M5.** `apps.src/prooftrans.c:575-578`, `char s[100]; sprintf(s, "%s%s_%d%s",
  q, label, ++n, q)`. **Ports directly.** Size the buffer from
  `strlen(label)` with malloc, write it with snprintf, and free it after
  `set_string_attribute`.
- **D1.** `apps.src/prooftrans.c:465-470` and `directproof.c:713-718`.
  `s[strlen(s)] != '\n'` is still always true, and `char c` never equals EOF.
  The 2026 code already checks the `fgets`/`fscanf` returns (defect (c)), but
  (a) and (b) remain, so the drain loop spins at EOF and appends to a
  String_buf forever. **Ports directly**, with an empty-string guard: test
  `s[strlen(s)-1]`, make `c` an `int` and stop on EOF. Also add the old
  NULL-term check after `read_term` (`prooftrans.c:476`), which becomes
  reachable once the loop terminates.
- **D2.** `prooftrans.c:456/472/499` and `directproof.c:704/720/737`:
  `fscanf(fin, "%s", s)` into `char s[BUF_MAX]`. **Ports directly**: use the
  `TOKEN_FMT` ("%1000s" via stringify) and `char s[BUF_MAX+1]`.
- **M7.** `ladr/dollar.c:69-`. It still sizes `Op_codes` from
  `greatest_symnum()` before the 26 `str_to_sn()` calls that create symbols;
  the only change from 2009 is `safe_calloc`. **Ports directly**: intern the
  operator table first, then size and fill both arrays.
- **M8.** `ladr/flatterm.c:152`, `while (fi != f->end->next)`, identical to
  2009. **Ports directly**: hoist `Flatterm stop = f->end->next;` above the
  loop.
- **N1/N2/N3.** `ladr/options.c:850`, `int dval = p->multiply ? p->val * val :
  p->val;` then `assign_parm()` range-checks it fatally. **Ports directly**:
  leave negative sentinels unscaled, multiply in a wider type and saturate,
  and clamp the derived value to the dependent parm's range.
- **U2/U3** (by inspection). `provers.src/giv_select.c:113` and `:561`. Port
  the `add_parts()` saturating helper into `current_cycle_size`, and make
  `iters` a `double` compared in double.
- **M9 (max_megs half), M10, M12.** `mace4.src/msearch.c:515-546`
  (`init_for_domain_size`) now guards the int wrap and allocates through
  `safe_malloc`, which fails cleanly, but nothing compares the table size
  against `max_megs`. `negpropindex.c:63-97` is the same (`safe_calloc`,
  unaccounted). `msearch.c:1525` still calls `disable_max_megs()`. **Ports
  with adaptation.** The old commits (`9dba5c2`, `82fb104`, `dc5d85c`) total
  cells plus negprop `(d+1)^arity` in 64-bit before allocating, refuse with
  `max_megs_no`, and re-enable LADR's limit through
  `set_max_megs(parm(opt->max_megs))` with a re-entrancy-safe exit proc. The
  2026 code differs in shape (`safe_malloc`, a new guard, the `-cores`
  scheduler), so those hunks need re-placing rather than applying. Note
  2026 mace4's default `max_megs` is 49152 (48 GB).
- **M11.** `ladr/memory.c:315` `megs_malloced()` still returns palloc alone.
  `get_mem` blocks of 500+ pointers go to `big_get`/`safe_malloc`
  (`:218-222`) uncharged. **Ports with adaptation**: the 2026 tree already
  counts `Total_allocated` in `safe_malloc`. Charge large blocks when first
  allocated, not on `big_get` cache reuse, and add them to
  `megs_malloced()` and the palloc max_megs check (old commit `8d352ef`).
- **T4, T5, T6, T8** (`tptp_to_ladr` only). `ladr/tptp_trans.c:377-401`
  (`tptp3_to_ladr_term`) is identical to 2009, the label quoting is at
  `:439`, and `declare_tptp_input_types` (`:153-187`) still lacks `~&`/`~|`.
  **All port directly**: the T4 converter fix (peel `~` off the quantifier
  prefix, re-apply it outside, reject non-`!`/`?`), T5 re-quoting, T6
  distinct objects (`be9486c`, `provers.src/tptp_to_ladr.c`) and T8
  (`49523d6`, 9 lines). The 2026 native reader (`prover9 file.p`) gets all
  four right.
- **T7.** Both readers are wrong. `provers.src/tptp_to_ladr.c:38` accepts
  `include/2` and reads the whole file; **the old fix ports directly**
  (`be9486c`). The native reader at `ladr/tptp_parse.c:1556-1567` parses the
  name list and then deliberately includes everything ("For simplicity,
  include everything (name filtering is rarely used in practice)"), and
  `scan_tptp_input` (~`:2026`) skips the list outright. This needs **new
  code**, not a port: filter `temp_assumps`/`temp_goals` by each formula's
  name before `plist_cat`. The names are retained, since the TSTP proof
  prints `file('M.ax',i2)`.
- **X1-X5.** `utilities/{attack,looper,gvizify,prover9-mace4}` are unchanged
  from 2009. **Ports directly**: `#!/usr/bin/env python3`, `print()`,
  `Popen(..., text=True)`, raw-string regexes, `chmod +x gvizify`.

## Sanitizer and poison method notes

- The ASan builds were made from clean copies (`rsync --exclude .git
  --exclude '*.o' --exclude '*.a'` plus `make realclean` for 2026, `git
  archive` for the old trees). I verified instrumentation by compile-line
  count and by `nm` on every `libladr.a` member, which was the old project's
  lesson. The 2026 Makefiles take `XFLAGS`, which reaches both compile and
  link lines; `make all -j8 XFLAGS=...` works. The 2009-era Makefiles race
  `install` against the link under `-j`, so `ladr/` was built with `-j8` and
  the rest serially. GCC's `-fsanitize=undefined` omits
  `float-cast-overflow`, so it was added explicitly for U3.
- **M8 and the old detection method.** FIXES.md found M8 with
  `-DPOISON_FREED_MEM`, a hook the old project *added* to `ladr/memory.c`.
  The 2026 `memory.c` has no such hook, so the old method cannot be applied
  without editing sources. The same effect is available without any source
  edit: `wrap/poison_wrap.c` is linked with `-Wl,--wrap=free_mem`. Every
  cross-object `free_mem()` call (`free_flatterm` and the rest) then goes
  through a wrapper that fills pointers 1..n-1 of a pooled block (n < 500)
  with `0xdeadbeefdeadbeef` before calling the real `free_mem`, which is
  exactly what the old `#ifdef` does. Validated both ways: the 2009 poisoned
  build SIGSEGVs and the old-fixed poisoned build proves. The 2026 poisoned
  build SIGSEGVs at `zap_flatterm (flatterm.c:152) <- fdemodulate <-
  fdemod_clause <- demodulate_clause <- cl_process_simplify <-
  index_and_process_initial_clauses`.
- `test.src/audit_alloc.py` is a static lister of unchecked
  `malloc`/`calloc` calls. It needs no source edits and can run on the 2026
  tree, but it is an audit, not a reproducer, so it was not part of this run.
- **M7 is visible on the plain 2009 build here.** glibc 2.43's heap check
  aborts (`Fatal glibc error: malloc.c:2371 (sysmalloc): assertion failed`).
  FIXES.md saw nothing without a sanitizer, presumably because of a different
  heap layout. On 2026 it is invisible without ASan or valgrind, because
  `safe_calloc` adds a header and changes the layout.

## Incidental findings (not old fixes)

- **The 2026 prover9 stalls in clausification on ALG033+1.** The translated
  `alg033.in` and the native `.p` both take more than 120 s. The 2009 build
  finishes the whole run, search included, in 41 s. A gdb sample puts it in
  `cnf.c distribute -> simplify_and_share -> remove_subsumed_share ->
  prop_subsume_share -> formula_ident_share`, with `safe_malloc(16000)` per
  comparison (`cnf.c:207`). With 300 s it does finish, at 226 s.
- The system `timeout` is uutils (Rust coreutils). It re-raises the child's
  fatal signal, so rc 139/134 is the child's own signal. systemd-coredump
  logged the crashes but stored no cores (`ulimit -c 0`; storage "none").

## Not runtime tests (skipped)

B1-B4 are build-only. These are hygiene or hardening: M4 (names bounded by
MAX_NAME 51, unchanged in 2026), M6, F2 and S1-S5. Performance-only: PF1-PF4,
PF6-PF10 and CXX. L1 is dead code, unfixed in the old tree too. R1 is the old
project's own regression, introduced by its PF10, and does not apply to 2026.

## Safety

This tree and the LADR-2026 clone were
only read. No file in either tree, and nothing under `/opencycdb`, is newer
than this run's start marker. `git status` in both is unchanged:
`?? resume.session.dpt` in the old tree, clean at `62ae6ff` in the new. The
only git use was `git archive` (read-only). There was no sudo and no package
installs. Builds ran at `-j8`; at most 6 case runners ran at once, each serial;
nothing used `-cores` or `-comp`. Every run had `timeout`, and plain builds
had `ulimit -v 16 GB` (4 GB for the D1/D2 cases).

---

Copyright (C) 2026 Chris Arena, 2026-10-27.  Written in collaboration
with Claude Opus 5.5 (Anthropic).  GPL-2.0-only; see `../COPYING`.  Not part
of the LADR-2009-11A release.
