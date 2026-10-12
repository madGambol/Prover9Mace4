# Prover9 / Mace4 (LADR-2009-11A) — bug survey and fixes

Upstream source: `LADR-2009-11A.tar.gz` (4 Nov 2009), the current release from
<https://www.cs.unm.edu/~mccune/prover9/download/>, unpacked into this
directory. `.src-download/` keeps the original tarball
(sha256 `c32bed5807000c0b7161c276e50d9ca0af0cb248df2c1affb2f6fc02471b51d0`).

Upstream's `README.make` says the build was tested with GCC 3.3.3 – 4.1.1.
This tree was built and audited with **GCC 15.2.0 on x86-64 Linux**, which
defaults to `-std=gnu23`.

## Summary

| # | What | Where | Severity |
| --- | --- | --- | --- |
| B1,B2 | K&R definitions rejected by C23 — **build fails** | `mace4.src/select.c`, `msearch.c` | blocker |
| B3 | `-lm` before the objects; absent in two dirs — **link fails** | 3 Makefiles | blocker |
| B4 | `make` renames the mace4 binary over a missing `bin/` | 4 Makefiles | blocker |
| M1 | Stack buffer overflow, `mace4 -n 10000000` **aborts** | `mace4.src/msearch.c:891` | memory safety |
| M2 | Stack buffer overflow on a long input filename | `ladr/top_input.c:892` | memory safety |
| M3 | Same, in 4 more programs — in `tptp_to_ladr` the name comes from a TPTP `include()`, so it is reachable from a problem file, not just the command line (**T1**) | `prooftrans`, `directproof`, `interpformat`, `tptp_to_ladr` | memory safety |
| M4 | Two more unbounded message buffers | `ladr/termorder.c`, `ladr/options.c` | hardening |
| M5 | Stack buffer overflow from `prooftrans -label`; **aborts** | `apps.src/prooftrans.c:481` | memory safety |
| M6 | All 30 remaining `sprintf` → `snprintf`, buffers sized to the widest output; exposed a too-small buffer in `interp.c` | whole tree | memory safety |
| M7 | **Heap buffer overflow on every production-mode run** — `Op_codes` is sized before the symbols that index it are created | `ladr/dollar.c:68` | memory safety |
| F1 | Input used as a `printf` format — **segfaults** | `ladr/fastparse.c:186` | memory safety |
| F2 | Non-literal formats in the model printer | `mace4.src/print.c` | hardening |
| D1 | Truncated proof ⇒ **infinite loop**, then NULL deref | `prooftrans.c:368`, `directproof.c:713` | hang / crash |
| D2 | Unbounded `fscanf("%s")` from untrusted input ⇒ **stack buffer overflow / segfault** | `prooftrans.c`, `directproof.c`, 3 sites each | memory safety |
| U1 | Signed overflow on **every default prover9 run** | `provers.src/giv_select.c:152` | UB |
| U2 | Same overflow where the value is actually used | `provers.src/giv_select.c:112` | wrong results |
| U3 | `double`→`int` conversion defeats `sos_limit` | `provers.src/giv_select.c:462` | wrong results |
| N1 | `assign(max_minutes,-1)` **aborts the prover** | `ladr/options.c:713` | usability |
| N2 | Signed overflow in the parm multiplier | `ladr/options.c:713` | UB |
| N3 | `assign(pick_given_ratio,-1)` **aborts the prover** | `ladr/options.c:713` | usability |
| S1 | Spurious outer loop, correct only by accident | `mace4.src/print.c:107` | latent |
| S2–S4 | `\|` for `\|\|`, `BOOL` holding a non-`BOOL` enum, pointer→`int` | `clash.c`, `paramod.c`, `flatdemod.c`, `avltree.c` | hygiene |
| S5 | Arithmetic on `void *` in the allocator — a GCC extension, not standard C | `ladr/memory.c:58,78` | portability |
| **T4** | **Wrong answers from `tptp_to_ladr`: the negation of a quantified formula was silently dropped.** `~ ! [X] : p(X)` translated to `p(X)` — the opposite of the input. 1,930 of the 17,912 first-order TPTP v9.3.1 problems contain the shape | `ladr/tptp_trans.c:398` | **wrong results** |
| T5 | `tptp_to_ladr` emitted single-quoted TPTP formula names into `label(...)`, which LADR's own reader rejects, so the translator produced files its own prover could not read. 382 problems affected | `ladr/tptp_trans.c:437` | correctness |
| T2 | The five messages that report a filename were sized by a round number (1024), so a long name was reported truncated; sized from `PATH_MAX` + 20% | `ladr/header.h`, 5 call sites | hardening |
| M8 | **Use-after-free in `zap_flatterm`, on every call** — the loop test re-reads `f->end` after the first iteration has freed `f`. Hit by the default prover9 run; invisible to valgrind and ASan because the block goes to an avail list, not to libc | `ladr/flatterm.c:152` | memory safety |
| T3 | ...and that put a 4953-byte array in five frames, one of them recursive, cutting circular-`include()` depth from 40262 to 1651. The message is now measured with `snprintf(NULL,0,...)` and allocated to fit: no array, no truncation at any length, depth 87255 | `ladr/fatal.c`, 5 call sites | hardening |
| PF1 | `String_buf` append is O(n²) | `ladr/strbuf.c` | performance |
| X1–X5 | All 4 installed helper scripts are unrunnable | `utilities/` | broken tooling |
| L1 | `demod1` drops a term its callee may have freed | `ladr/demod.c:376` | latent, reported only |

All fixed except **L1**, which is in a function with no callers anywhere in the
tree and so cannot be validated; it is written up instead.

New tests: `make -C test.src check` runs 35 regression tests — 34 on a plain
build, since the M7 overflow is visible only under a sanitizer and that test
reports SKIP rather than a meaningless PASS — of which 27 fail on the baseline
(upstream with only the build fixes applied, since upstream alone does not
compile). Plus a `String_buf` unit test and a Mace4 soundness check that
independently verifies 271 reported models against their input clauses.

One **intended output change** on ordinary input: `prooftrans` and
`directproof` now emit one blank line, not two, between a proof's comments and
its first clause. That blank line was being swallowed and reprinted by the D1
defect; see "Visible output change" under D1.

## How the survey was done

| Method | Result |
| --- | --- |
| Build with GCC 15, then `-Wextra -Wformat=2 -Wshadow` | 3 hard errors, 39 warnings → 23 (all remaining are benign unused-variable/debug-helper warnings) |
| ASan + UBSan build, 168-file corpus (upstream manual + web examples) through `prover9` and `mace4` | 1 distinct UBSan finding (`giv_select`, hit by 135 of 168 files), 0 ASan findings; `mace4` entirely clean |
| The same ASan + UBSan sweep re-run **after** the fixes, and again after the `snprintf` conversion | 0 findings both times — but see the correction below: **these three sweeps were only partly instrumented and their ASan result was not trustworthy** |
| ⚠️ **Correction.** Auditing those sweeps found only **1 of the 77 objects in `libladr.a`** instrumented. Root cause, from the build logs: the sanitizer build compiled **60** files where a full build compiles **137**, and **none** of them were in `ladr/` — that tree had been created by copying an already-built tree, so `make` saw the library objects as current and never recompiled them with the flags. The missing 77 are exactly `libladr.a`. ASan only checks accesses in code it instrumented, so nearly every memory access in the library — `term.c`, `memory.c`, `parse.c`, `symbols.c`, `strbuf.c` and the rest — went unchecked. | the earlier "0 ASan findings" was **substantially vacuous**; the UBSan result stands only for `mace4.src` and `provers.src`, which were instrumented |
| The check that was supposed to catch this — `nm` for `__asan_init` in the binary — is worthless for the purpose: linking the ASan runtime says nothing about which *objects* were instrumented. The positive control was equally misleading, because it used a `giv_select.c` defect, and `provers.src` was one of the directories that *did* get the flags. A control has to cover the code the claim is about. | lesson recorded rather than quietly fixed |
| Rebuilt from `git archive` (sources only, no stale objects) plus `make clean`, then verified two ways before trusting any result: **137 of 137** compile lines carried `-fsanitize`, and **77 of 77** `libladr.a` objects reference `__asan` | finding **M7**, a heap buffer overflow in `ladr/dollar.c` reached by every production-mode example in the manual |
| That corrected sweep, over the same 168 files per program, **before** the M7 fix | `prover9` **5 findings, all of them M7**; `mace4` **0 findings**; no UBSan findings. So the broken sweeps had concealed exactly one defect, not a collection |
| The corrected sweep re-run on the finished tree, 168 files × 2 programs = **336 instrumented runs**, repeated after each subsequent change | **0 findings** every time — no ASan report, no UBSan report, no signal, no unexpected exit status. This is the number that replaces the retracted one above. Run serially it took 61 minutes for `prover9` alone; at 24-way parallelism the whole 336 take 23 |
| Positive control for *that* zero, on the same build: `ladr/dollar.c` reverted to its pre-fix state and nothing else, then rebuilt | the control binary reports `ERROR: AddressSanitizer: heap-buffer-overflow` on the M7 input; the shipped binary is clean on it. A defect in `ladr/` would have been seen, which is the claim the earlier control failed to establish |
| Full regression suite run *against the instrumented build* rather than a plain one | **32 of 32 pass**, including the sanitizer-gated M7 test that reports SKIP on a plain build |
| After deleting upstream's 83 unbuilt backup files, the instrumented build still compiles **137 of 137** files — the same count as before | direct evidence that nothing load-bearing was removed: the set of compiled translation units is unchanged |
| Valgrind memcheck with `--track-origins=yes` over the corpus, 168 files × 2 programs, on the **shipped** binaries with no instrumented build | **0 memory errors** in 336 runs: no invalid access, no use of uninitialised memory, no bad free. This is the first check of uninitialised memory in the project — ASan does not detect it, and MSan would need every dependency including libc rebuilt |
| The same sweep with leak checking, which the ASan sweeps had switched off (`detect_leaks=0`) | **96 leaks, all in `mace4`**, `prover9` clean on all 168: 32 bytes per run from `mace4()`'s `malloc(sizeof(struct mace_results))`, which `mace4.c` and `mprover.c` never free, plus one 6,416-byte case. Reported, not fixed: the struct is leaked once immediately before the process exits |
| ⚠️ **The limit of both tools here.** `free_mem()` returns a block to libc only when it is 500 pointers or more; every term, clause and literal goes onto the `M[n]` avail list instead and stays owned by the process. Valgrind and ASan therefore see reused LADR objects as live and valid, and cannot report a use-after-free of one | the clean use-after-free result above covers blocks ≥ 4000 bytes and the 56 direct `free()` calls — **not** LADR's own recycling. Closing that gap with `-DPOISON_FREED_MEM` found **M8** immediately |
| 350 rounds of mutational fuzzing of both parsers | 0 new memory errors — only the same `giv_select` overflow |
| Targeted review of every `sprintf`, format-string, `strlen()` index, `double`→`int` conversion, and loop-variable reuse | findings M1–M4, F1–F2, D1, U3, S1 |
| Line-by-line review of the committed diff, then malformed-input fuzzing of `prooftrans`/`directproof` (34 inputs × 2 tools × 6 formats = 408 runs under ASan+UBSan) | finding **D2**, which the corpus sweep could not reach: it drives only `prover9` and `mace4`, and the earlier targeted app checks used well-formed proofs |
| Exhaustive `sprintf` → `snprintf` conversion with each buffer sized to its format's widest output | finding M5 (missed on the first pass) and a too-small buffer in `interp.c` |
| Rebuild with `-Wformat-truncation=2 -Wformat-overflow=2 -Wstringop-overflow=4` | **no truncation or overflow warnings**: every destination is provably large enough |
| `make test1` / `test2` / `test3` | pass before and after |
| A/B diff of normalised output over the whole corpus, baseline vs fixed | see "Behaviour preservation" |

The parser and the core term/clause machinery held up well: the fuzzing and
the ASan corpus sweep found nothing in them. Every memory-safety bug below is
in error-reporting or report-printing code, not in the inference engine.

The 23 warnings that remain after the fixes were each checked and are benign:

- `declare_base_symbols` (`symbols.c`), `complex4_p1` (`complex.c`) — return
  values deliberately discarded; the calls are made for their side effects.
- `transpose_binary` (`interp.c`) — ignores `str_to_int`'s status, but the
  function is documented as requiring a well-formed interpretation.
- `mindex_retrieve_next` (`mindex.c`) — a stale `Trail tr;` declaration; the
  code uses `pos->tr`.
- `factor2_instances` (`ivy.c`) — `l1i` is the literal the factoring step
  *keeps* (only `l2i` is dropped), so computing it and not using it is correct.
- `check_flat` / `check_flat2` (`discrimw.c`, `discrimb.c`) — debug helpers
  kept for use under a debugger.
- the loop counters and `props_attr` / `label_attr` — unused leftovers.

`demod1` (`demod.c`) is the one that is *not* benign; it is written up as L1
below.

---

## Build blockers (prevent compilation at all)

### B1, B2 — K&R function definitions rejected by C23
`mace4.src/select.c:236`, `mace4.src/msearch.c:850`

```c
int select_concentric_band(min_id, max_id, max_constrained)   /* no types */
int next_domain_size(n)
```

Old-style definitions with implicit `int` parameters. GCC 14+ makes
`-Wimplicit-int` an error by default, so the build stops here.

**Fix** — give the parameters their types. No behavioural change: the
implicit type was already `int`.

### B3 — `-lm` before the object files, and missing entirely in two directories
`provers.src/Makefile` (7 rules), `mace4.src/Makefile`, `apps.src/Makefile`

```
$(CC) $(CFLAGS) -lm -o prover9 prover9.o $(OBJECTS) ../ladr/libladr.a
```

A static library only satisfies references from objects listed *before* it, so
`-lm` placed first is discarded:

```
search.c: undefined reference to `round'
avltree.c: undefined reference to `ceil'
```

`mace4.src` and `apps.src` never linked `-lm` at all, even though
`libladr.a(avltree.o)` calls `ceil`.

**Fix** — `-lm` last on every link line, and added to the `mace4.src` and
`apps.src` rules. (`test.src/Makefile` already had it right, which is how the
inconsistency is visible.)

### B4 — `make` destroys `bin/` when it does not exist
`Makefile`, `mace4.src/Makefile`, `apps.src/Makefile`, `provers.src/Makefile`

`mace4.src`'s install step is `/bin/mv mace4 ../bin`. With no `bin/` directory
present, `mv` does not fail — it **renames the mace4 binary to a file called
`bin`**, after which every later `cp ... ../bin` fails with
`target '../bin': Not a directory`. A fresh checkout that does not carry an
empty `bin/` cannot be built, and the failure message points nowhere near the
cause.

**Fix** — `/bin/mkdir -p ../bin` ahead of each install step, and `mkdir -p bin`
in the top-level `all`.

---

## Memory-safety bugs

### M1 — stack buffer overflow in Mace4's domain-size banner *(confirmed with ASan)*
`mace4.src/msearch.c:891`

```c
char str[20];
sprintf(str, "DOMAIN SIZE %d", n);
```

`"DOMAIN SIZE "` is 12 characters, so a 10-digit `n` needs 23 bytes. `n` is the
domain size, settable straight from the command line, and the `sprintf` runs at
the top of the loop before any search work:

```
$ mace4 -n 10000000 -f x.in                      # pristine, ASan build
AddressSanitizer: stack-buffer-overflow ... WRITE of size 21
  [32, 52) 'str' (line 891) <== overflows this variable
  #3 mace4 mace4.src/msearch.c:892

$ mace4 -n 10000000 -f x.in                      # pristine, ordinary build
*** buffer overflow detected ***: terminated     # SIGABRT, rc=134

$ mace4 -n 10000000 -f x.in                      # fixed
============================== DOMAIN SIZE 10000000 ==================
```

Note the ordinary build: glibc's `_FORTIFY_SOURCE` checking, on by default on
current distributions, turns this into a hard abort, so it is not a latent
problem on a modern system.

**Fix** — `char str[64]` and `snprintf`.

### M2 — stack buffer overflow on a long input filename *(confirmed with ASan)*
`ladr/top_input.c:892`

```c
char s[100];
sprintf(s, "read_all_input, file %s not found", argv[i]);
```

The literal text is 31 characters, so any missing filename longer than 68
characters overflows. `read_all_input` is the shared command-line entry point,
so this reaches **prover9, mace4 and every app**:

```
$ prover9 -f zzzz...(200 chars)
AddressSanitizer: stack-buffer-overflow ... WRITE of size 232
  [176, 276) 's' (line 892) <== overflows this variable
  #3 read_all_input ladr/top_input.c:893
```

**Fix** — `char s[1024]` and `snprintf`.

### M3 — the same pattern in four more programs
`apps.src/prooftrans.c:295`, `apps.src/directproof.c:668`,
`apps.src/interpformat.c:217`, `provers.src/tptp_to_ladr.c:49`

All four do `char s[100]; sprintf(s, "file %s not found", argv[...])`.
Confirmed with ASan for `prooftrans` (232-byte write into `s[100]`); the others
are the identical construct. `tptp_to_ladr`'s copy takes the filename from an
`include()` directive **inside the input file** rather than from `argv`.

**Fix** — `char s[1024]` and `snprintf` in all four.

### M5 — stack buffer overflow from `prooftrans -label` *(confirmed with ASan)*
`apps.src/prooftrans.c:481`

```c
char s[100];
char *q = (ordinary_constant_string(label) ? "" : "\"");
sprintf(s, "%s%s_%d%s", q, label, ++n, q);
```

`label` is `argv[n+1]` of the `-label` option, so it has no length limit. A
label longer than about 85 characters overflows the buffer:

```
$ prooftrans hints -label <300 chars> < proof.txt     # pristine, ASan
AddressSanitizer: stack-buffer-overflow ... WRITE of size 153
  [96, 196) 's' (line 481)
  #3 main apps.src/prooftrans.c:484

$ prooftrans hints -label <300 chars> < proof.txt     # pristine, ordinary build
*** buffer overflow detected ***: terminated          # SIGABRT, rc=134
```

This one was missed on the first pass, which only looked for the
`"file %s not found"` shape; it turned up in the follow-up sweep that
converted every remaining `sprintf`.

**Fix** — the label length is only known at run time, so the buffer is sized
from it (`malloc`) rather than guessed, and written with `snprintf`.
`set_string_attribute` interns a copy of the string, so the buffer is freed
immediately afterwards.

### M7 — heap buffer overflow in `$`-evaluation setup *(confirmed with ASan)*

The oldest defect in this document, and the one that says the most about how it
was found. `ladr/dollar.c:68` sizes an array from the symbol count, then fills
it using calls that *add* to that count:

```c
Symbols_size = greatest_symnum() + 1;          /* a snapshot: 111 here */
Op_codes = calloc(Symbols_size, sizeof(int));  /* 444 bytes */

Op_codes[str_to_sn("+", 2)]   = SUM_OP;
Op_codes[str_to_sn("mod", 2)] = MOD_OP;        /* ... 26 of these */
```

`greatest_symnum()` just returns `Symbol_count` (`ladr/symbols.c:705`), while
`str_to_sn()` *interns* a symbol that is not already in the table, returning a
symnum past the snapshot. Any of the 26 operator symbols that the input did not
already mention is created right here, and its op code is written past the end
of `Op_codes` — up to 26 `int`s, 104 bytes. The size of the allocation it runs
past depends on the input, since the symbol count does: 444 bytes (111 symbols)
on the corpus example quoted below, 324 (81 symbols) on the minimal reproducer
in `regress.sh`. `Rules`, allocated from the same stale count a few lines later,
is undersized for the same reason; it escapes only because every write to it is
range-checked.

The author knew about the hazard: lines 144, 169, 191, 372 and 424 of this same
file all guard with `if (symnum >= Symbols_size)`. The initialisation block is
the one place that does not.

```
ERROR: AddressSanitizer: heap-buffer-overflow
WRITE of size 4 at 0x78e3f31e09fc
    #0 init_dollar_eval ladr/dollar.c:75
    #1 index_and_process_initial_clauses provers.src/search.c:2394
    #2 search provers.src/search.c:2710
0x78e3f31e09fc is located 0 bytes after 444-byte region
```

**Trigger:** `set(production)`, which implies `set(eval_rewrite)`, which is the
only caller of `init_dollar_eval`. In the corpus that is `2inverter.in`,
`cabbages.in`, `jugs.in`, `list.in` and `queens3.in` — **every production-mode
example in the Prover9 manual**. It is present in pristine LADR-2009-11A and,
independently confirmed, in the current LADR-2026 release.

**Confirmed a second time, by a second tool, on a binary that was never
instrumented.** Valgrind's memcheck reports the same defect on the *shipped*
baseline executable, with no rebuild of any kind:

```
Invalid write of size 4
   at 0x4034615: init_dollar_eval
   by 0x400BAF2: search
 Address 0x4d7d684 is 0 bytes after a block of size 324 alloc'd
   at 0x49F2BE3: calloc
   by 0x4034594: init_dollar_eval
```

This matters more than a duplicate finding usually would. The ASan result for
this defect had to be retracted once already because the library was not
actually instrumented (see the ⚠️ Correction), and the whole class of error
there was *"the tool silently did not examine the code I thought it did."*
Valgrind cannot fail that way: it instruments machine code as it runs, so there
is no build step to get wrong and no object that can be left out. The fixed
build is clean under the same command, including `--track-origins=yes`.

**Fix** — move the 26 `str_to_sn()` calls into a table, intern them all first,
then take the symbol count and allocate both arrays from it:

```c
for (i = 0; i < NUM_DOLLAR_OPS; i++)
  symnums[i] = str_to_sn(Dollar_ops[i].name, Dollar_ops[i].arity);

Symbols_size = greatest_symnum() + 1;
Op_codes = calloc(Symbols_size, sizeof(int));
Rules = calloc(Symbols_size, sizeof(void *));
...
for (i = 0; i < NUM_DOLLAR_OPS; i++)
  Op_codes[symnums[i]] = Dollar_ops[i].op_code;
```

**No change in results, and the reason matters.** The symbols that overflow are
exactly the ones the input never mentions — every symbol that appears in the
input is interned during parsing, before `init_dollar_eval` runs, so it already
has a symnum below the snapshot. The out-of-bounds slots are therefore never
read back: all five reads are range-guarded and reject them. Enlarging the
arrays does make those guards accept symnums 111–136, but nothing in any input
can refer to them. Checked empirically as well — `+`, `max`, `min`, `mod` and
`abs` evaluate identically before and after.

So this is a pure memory-corruption bug: it silently writes over whatever the
allocator placed after `Op_codes`. That is also why it survived 17 years.
Neither a plain build nor `MALLOC_CHECK_=3` reports anything on any of the five
examples — verified — so only a sanitizer sees it. `test.src/regress.sh` tests
it only when handed an instrumented build, and reports SKIP otherwise.

### M4 — two more fixed-size message buffers (hardening)
`ladr/termorder.c:433` (`char mess[200]` + a symbol name),
`ladr/options.c:239` (`char s[100]` + a flag name)

Symbol and flag names are bounded by `MAX_NAME` (51), so neither overflows
today. Converted to `snprintf` anyway so the bound is enforced locally rather
than depending on a constant three files away.

Also audited and found **safe**: `ladr/symbols.c` `next_skolem_symbol`
(prefixes are the literals `"c"`/`"f"`), `fresh_symbol` / `gen_new_symbol`
(explicit `MAX_NAME` guards), `ladr/random.c:92` (`symbol[MAX_NAME]`),
`ladr/just.c:1050` (`char s[21]`, max 12 written), and the three
`[strlen(x)-1]` indexings in `ladr/weight2.c`, `ladr/parse.c` and
`provers.src/tptp_to_ladr.c` (each provably reached with length ≥ 1).

---

### M6 — every remaining `sprintf` converted to `snprintf`
whole tree — 30 call sites

After the individual fixes above, the tree still had 30 `sprintf` calls. All of
them are now `snprintf`, with each destination sized to hold the widest output
its format can produce rather than a round number picked by eye.

A new constant documents the one recurring quantity:

```c
/* ladr/header.h -- bytes to hold any int formatted with "%d", NUL included:
   "-2147483648" is 11 characters, so 12. */
#define INT_STRING_SIZE 12
```

18 of the 30 were in the compiled programs. The other 12 were in `ladr/save/`,
`bob/` and `provers.src/save/` — upstream's own in-tree backups, which no
Makefile references. Converting those was a mistake: it edited dead code to
make `grep sprintf` come back empty, which is a cosmetic goal, and it produced
twelve changes that no build or test could exercise. Those directories have
since been deleted outright (see "Removed: upstream's in-tree backups"), which
disposes of the twelve edits along with them. The 18 that matter are unaffected.

Two functions write into a buffer their caller owns, which `sprintf` gave them
no way to bound. Both now take the size, so the compiler checks every call
site, and both reject a buffer that is too small instead of truncating —
a truncated variable name or clause-id label would be silently wrong output
in a proof, which is worse than a loud failure:

```c
void  symbol_for_variable(char *str, size_t size, int varnum);   /* was (str, varnum) */
char *jmap2(I3list map, int i, char *a, size_t size);            /* was (map, i, a)   */
```

That change exposed a real latent overflow: `ladr/interp.c` was passing
`char str[10]` to `symbol_for_variable`, which can write 13 bytes
(`"v-2147483648"`). Its buffer is now `MAX_VAR_SYMBOL_SIZE`.

The sizing is verified by the compiler rather than by inspection. Building with

```
-Wformat-truncation=2 -Wformat-overflow=2 -Wstringop-overflow=4 -Wformat=2
```

reports **no truncation and no overflow warnings** anywhere in the tree, i.e.
GCC can prove every destination is large enough for its format. The two
warnings this flushed out while the work was in progress were both acted on:
`jmap2`'s unbounded `size` parameter (now range-checked) and `prooftrans`'s
runtime size arithmetic (now written so the bound is obvious, with slack).

The five remaining `-Wformat-nonliteral` warnings are intentional and already
guarded: `ladr/string.c`'s `double_to_str` passes the configurable
`Float_format` and checks `snprintf`'s return against the buffer size, and
`mace4.src/print.c` chooses between the literals `"%2d"` and `"%3d"` at run
time.

## Format-string bugs

### F1 — input text used as a `printf` format
`ladr/fastparse.c:186-187`

```c
fprintf(stderr, s);
fprintf(stdout, s);
```

`s` is a line just read from the input file (`fast_read_term`), printed when a
term is missing its terminating period. Any `%s`/`%d`/`%n` in the input is
interpreted as a conversion specification — reading arbitrary stack as pointers.
This is the classic uncontrolled-format-string defect, and it is a live crash,
not a theoretical one:

```
$ printf 'f(%%s%%s%%s%%s%%s%%n,a)\n' > fmt.in

$ unfast < fmt.in ; echo "rc=$?"          # pristine
rc=139                                     # SIGSEGV, no output

$ unfast < fmt.in ; echo "rc=$?"          # fixed
f(%s%s%s%s%s%n,a)
Fatal error:  fast_read_term, term ends before period.
rc=1
```

**Fix** — `fprintf(stderr, "%s", s)`.

### F2 — non-literal formats in Mace4's model printer (hardening)
`mace4.src/print.c:114,119,137,145`

`printf(s2)` / `printf(s3)` where `s2`/`s3` are the constants `"--"`, `"---"`,
`" -"`, `"  -"`. Harmless today because none contains `%`, but it defeats
`-Wformat-security` for the whole file.

**Fix** — `printf("%s", s2)`.

---

## Hangs and crashes on truncated input

### D1 — `prooftrans` and `directproof` spin forever on a truncated proof
`apps.src/prooftrans.c:368`, `apps.src/directproof.c:713` — *three defects in
one place*

```c
fgets(s, BUF_MAX, fin);
...
if (s[strlen(s)] != '\n') {       /* (a) */
  char c;                         /* (b) */
  do {
    c = fgetc(fin);
    sb_append_char(comment, c);
  } while (c != '\n');
}
```

- **(a)** `s[strlen(s)]` is by definition the NUL terminator, so the test is
  `'\0' != '\n'` — **always true**. The intended test was `s[strlen(s)-1]`
  ("did this line end with a newline, or was it truncated by `BUF_MAX`?").
  Consequence: the drain loop runs after *every* `%` comment line and swallows
  the following line into the comment text.
- **(b)** `fgetc` returns `int`; storing it in a `char` loses the EOF
  distinction. At end of file `c` becomes `(char)-1`, never `'\n'`, so the
  `do/while` **never terminates**.
- **(c)** The `fgets` return value is discarded, so EOF immediately after the
  `%` token leaves `s` holding `"%"` and the enclosing
  `while (str_ident(s, "%"))` spins as well.

Reproduced by truncating any prover9 output right after a `%` comment line —
both tools spin at 100 % CPU with memory climbing until killed:

```
t=3s  state=R RSS=2444kB utime=306ticks
t=10s state=R RSS=2788kB utime=1007ticks
t=25s state=R RSS=3204kB utime=2508ticks
```

Fixing just those three exposed **two further pre-existing defects** in the
clause loop immediately below, which had been unreachable only because control
never got there:

- `read_term` returns `NULL` at EOF and the result goes straight into
  `term_to_clause(clause_term)` → **segfault** (observed once (a)–(c) were
  fixed).
- `rc = fscanf(fin, "%s", s)` leaves `s` untouched at EOF, so if the
  `"=========="` separator never arrives the loop spins — a second hang.

**Fix** — test `s[strlen(s)-1]` with an empty-string guard; make `c` an `int`
and stop at `EOF`; check the `fgets` and `fscanf` return values; reject a
`NULL` term. Truncated input now ends with a clean diagnostic:

```
$ prooftrans < truncated-proof
Fatal error:  proof ends prematurely (truncated input?)      [exit 1]
```

#### Visible output change: one blank line per proof

Defect (a) had a second, benign consequence that is worth stating plainly,
because it is the one place where fixed output differs from upstream on
ordinary input. Because the drain loop ran after *every* comment line, it
swallowed the line following the comment block — a blank line — into the
comment buffer and printed it back. Upstream therefore emits **two** blank
lines between a proof's comments and its first clause; the fixed build emits
**one**:

```
% Given clauses 12.
                      <- separator, from the proof file
                      <- upstream only: the line the drain loop swallowed
1 x * y = y * x # label(non_clause) # label(goal).  [goal].
```

Checked across both tools and all seven output formats, on a 1-proof and a
3-proof input: the *only* difference anywhere is one such blank line per
proof (so 3 lines for the 3-proof input), and the `hints` and `ivy` formats of
`prooftrans`, which do not reproduce the comment block, are byte-identical.
`test.src/regress.sh` pins this down.

This did not show up in the original corpus A/B diff because that compared
`prover9` and `mace4` only, not the apps built on top of them.

### D2 — unbounded `fscanf("%s")` into a fixed buffer *(confirmed with ASan)*

Found later than the rest, by reviewing the committed D1 diff line by line and
then fuzzing the two tools with *malformed* proofs. It sits in the same loop as
D1, in three places per file:

```c
char s[BUF_MAX];                      /* BUF_MAX is 1000 */
...
rc = fscanf(fin, "%s", s);            /* "%" or clause id */
```

`%s` has no inherent bound, so the length of the write is chosen by the input,
not by the program. `prooftrans` and `directproof` read a proof from stdin or a
named file, so this is attacker-controlled:

```
==3497123==ERROR: AddressSanitizer: stack-buffer-overflow
WRITE of size 5001 at 0x77ca07e00438 thread T0
    #2 __isoc23_fscanf
    #3 main apps.src/prooftrans.c:379
Address ... is located in stack of thread T0 at offset 1080 in frame
    #0 main apps.src/prooftrans.c:214
```

The overflow begins at exactly 1000 characters (1000 bytes plus the NUL
`fscanf` appends is one byte past `s`), and a 50 000-character token segfaults
the ordinary non-instrumented build.

**Fix** — give the conversion an explicit field width, derived from `BUF_MAX`
by stringification so the two cannot drift apart, and declare the buffer one
byte larger than the width to hold the NUL:

```c
#define STRINGIFY_(x) #x
#define STRINGIFY(x)  STRINGIFY_(x)
#define TOKEN_FMT     "%" STRINGIFY(BUF_MAX) "s"   /* -> "%1000s" */
...
char s[BUF_MAX+1];                 /* +1: TOKEN_FMT's width is BUF_MAX */
rc = fscanf(fin, TOKEN_FMT, s);    /* "%" or clause id */
```

Truncating is safe at these three call sites: the token read is either `"%"` or
an integer clause id, so anything approaching 1000 characters is malformed
input, and the existing checks reject it. Over-long input now exits 1 with
`Fatal error:  sread_term error` instead of smashing the stack.

**Verification.** 34 malformed inputs (over-long tokens at 998/999/1000/1001/
1002/5000 characters in each of four positions, plus truncations every 200
bytes) × 2 tools × 6 output formats = 408 runs under ASan+UBSan: **0 findings,
0 crashes**. The same 408 runs against the pre-fix build report **96**
stack-buffer-overflows, so the harness demonstrably can fail. On well-formed
input the fix changes nothing: 27/27 byte-identical across both tools, seven
formats, and 1-proof/3-proof inputs.

**Why the earlier sweeps missed it.** The ASan corpus sweep drives `prover9`
and `mace4`, which do not use this code; the targeted `prooftrans`/
`directproof` checks did exercise it, but only with well-formed proofs, and the
parser fuzzing targeted the term parsers rather than the proof-file reader. The
lesson is the one D1 already illustrated: an app reading its own output format
still needs to be fuzzed with input that violates that format.

Two further `scanf` sites were checked and left alone: `ladr/dioph.c` is inside
`#ifdef SOLO` and reads `%d`, and `apps.src/old/modelformat.c` is not compiled.
(`bob/prooftrans.c` had the same three unfixed sites, but that directory has
since been deleted — see "Removed: upstream's in-tree backups".)

### T1 — buffer overflow on a long filename, reachable from a TPTP file

`provers.src/tptp_to_ladr.c` formatted the name of a missing `include()` file
into `char s[100]`:

```c
fin2 = fopen(fname2, "r");
if (fin2 == NULL) {
  char s[100];
  sprintf(s, "read_tptp_input, file %s not found", fname2);   /* unbounded */
  fatal_error(s);
}
```

`fname2` comes straight out of the input file, so any TPTP problem containing
`include('<400 characters>')` overflows it. Upstream aborts with glibc's
`*** buffer overflow detected ***`, exit 134.

This is the same shape as M2/M3 and was fixed in the same pass, but it is worth
separating out, because of **where** it lives. `tptp_to_ladr` and `ladr_to_tptp`
are two of the 40 programs `make all` installs, and *nothing* in upstream —
`make test1`, `test2`, `test3`, or any file in `test.src/` — ever runs either
one. The defect was therefore fixed without a single test touching the program
it was in. That gap is now closed: `regress.sh` drives a TPTP problem through
`tptp_to_ladr`, proves the translation with `prover9`, and translates it back
with `ladr_to_tptp`.

**Upstream did write a TPTP test, and it has not compiled for years.**
`test.src/tptp_test.c` reads a TPTP file and prints the formulas it parsed —
exactly the coverage that was missing. It has its own rule in
`test.src/Makefile`, but it is absent from that file's `PROGRAMS` list
(`avltest strbuf_test`), so neither `make all` nor `make check` builds it, and
nothing noticed as the library moved out from under it. It calls three
functions that exist nowhere in the tree — not in `ladr/`, not in any header,
0 definitions and 0 declarations each:

| Called by `tptp_test.c` | Status |
| --- | --- |
| `declare_tptp_parse_types()` | renamed; it is `declare_tptp_input_types()` now |
| `tptp_formula()` | gone; formula conversion is `tptp_input_to_ladr_formula()` |
| `read_tptp_file(stdin)` | gone; reading moved into the translator programs |

`make -C test.src tptp_test` fails with four hard errors under C23, and fails
**identically on the baseline tree**, so this is upstream rot rather than
anything the changes here caused. The deleted `ladr/save/read_tptp_file.c` is
not the missing piece either: it was never compiled, and its signature
(`FILE *, FILE *, BOOL, int`, returning `void`) does not match the one-argument
call returning `Plist` that the test makes. The test, the saved copy and the
shipped library are three different generations of the same API.

It is left in place and unrepaired. Repairing it would mean writing the two
missing functions, which is new code rather than a fix, and the coverage it was
for now exists in `regress.sh` against the programs that are actually
installed.

### T2 — the filename buffers were big enough to be safe, but not to be useful

The fix above, and the four sibling sites that format a filename into a
message, used `char s[1024]` with `snprintf`. That is safe — `snprintf`
truncates rather than overflowing — but 1024 is a round number rather than a
reasoned one, and a name longer than about a kilobyte came back cut off in the
middle, which is exactly when a user most needs to see the whole thing.

Every one of these messages names a file that was just passed to `fopen()`, and
no path longer than `PATH_MAX` can be opened, so `PATH_MAX` is the real bound on
the longest name worth reproducing in full. The five sites now share one size
derived from that, rather than five copies of a guess:

```c
/* ladr/header.h */
#define FILE_MESSAGE_SIZE (((PATH_MAX + 32) * 6) / 5)
```

`PATH_MAX` (4096 on Linux, with a fallback for systems that leave it
undefined), plus 32 for the longest fixed text any of the messages wraps around
the name, plus 20% margin — 4953 bytes, all integer arithmetic so it remains an
integer constant expression and can size an array. `fatal_error` itself has no
buffer, it just `fprintf`s what it is handed, so the larger size genuinely
reaches the output.

Two `regress.sh` tests pin this, and both match the entire message rather than
just the name — the first version of the `prover9` check grepped only for the
name and **passed against the unfixed baseline**, because prover9's banner
echoes `argv` before it ever gets to the error message.

### M8 — use-after-free in `zap_flatterm`, on every call

Found by poisoning LADR's own allocator, which is the only thing that can see
it. The loop was:

```c
Flatterm fi = f;
while (fi != f->end->next) {     /* re-evaluated every iteration */
  Flatterm tmp = fi;
  fi = fi->next;
  free_flatterm(tmp);            /* the first iteration frees f itself */
}
```

The first pass frees `f`, and every subsequent evaluation of the test reads
`f->end`, then `f->end->next`, through a block that has already been released.
The fix hoists the stopping point out of the loop, so the same expression is
evaluated once while `f` is still valid.

**Why no tool saw it.** `free_flatterm()` ends in `free_mem()`, and `free_mem()`
only hands a block back to libc when it holds `MAX_MEM_LISTS` (500) pointers or
more — 4000 bytes. Anything smaller, which is every term, clause and literal,
is linked onto the `M[n]` avail list and stays owned by the process. To valgrind
and to ASan that memory is live and validly addressable, so reusing it is not an
error they can report; the freed flatterm simply keeps its contents until
something else claims the slot. A custom pool allocator makes both tools blind
in exactly the region where the bugs live. In this tree that is 56 of the 57
`free()` calls' worth of objects.

**What makes it visible.** `ladr/memory.c` now takes `-DPOISON_FREED_MEM`, which
fills a block with `0xdeadbeefdeadbeef` as it goes onto the avail list, leaving
only the first pointer, which is the list link. The pattern is non-canonical as
an x86-64 address, so a dereference faults at once instead of happening to work.
Off by default: `free_mem()` is a hot path, which is why `mega_mem_calls()`
exists to count the millions of calls a search makes.

With it on, the defect is immediate and unambiguous:

```
Invalid read of size 8
   at zap_flatterm (flatterm.c:152)
   by fdemodulate (flatdemod.c:408)
   by fdemod_clause (flatdemod.c:483)
   by demodulate_clause (demodulate.c:150)
   by cl_process_simplify (search.c:1155)
   by index_and_process_initial_clauses (search.c:2432)
 Address 0xdeadbeefdeadbeff is not stack'd, malloc'd or (recently) free'd
```

The faulting address is the poison plus 16, and `next` sits at offset 16 — the
loop read `f->end->next` with `f->end` already poisoned.

**Evidence, in the order it was gathered.**

| Check | Result |
| --- | --- |
| Poisoned build, before the fix, `prover9 -f prover9.examples/x2.in` | SIGSEGV; `make test1` fails |
| Same tree, same flags, **only this loop reverted** vs. fixed | `SIGSEGV` vs. `THEOREM PROVED` — the one line carries the whole effect |
| Poisoned build, after the fix, corpus sweep 168 files × 2 programs | **336 runs, 0 crashes** |
| Poisoned build, after the fix, full regression suite | **31 of 31 pass** — this is what covers `prooftrans`, `directproof`, the TPTP translators and the helper scripts, which the corpus sweep never drives |
| Behaviour preservation, before vs. after, over the corpus | 0 differences — see the note on noise below |

**A note on measuring that last row**, because the first two attempts were
worthless. The naive A/B reported 336 of 336 differing; the cause was the banner
line `The command was "..."`, which echoes each binary's own path — the same line
that had already made a draft T2 test pass against the unfixed baseline. With
that filtered, 154 of 336 differed, which still proves nothing: running the
*same* binary against itself differed on 172 of 336, because at 80-way
parallelism the problems that stop on a wall-clock limit do a different amount of
work each time. Two self-comparison passes identified 162 runs that are
deterministic, and over those the before/after comparison differs on **0 of 162**.
A control that reports more differences than the thing being measured is the
only reason that number means anything.

**Are there others like it?** A detector that looks for loops whose test
dereferences a pointer the body frees, following simple assignment aliases into
the enclosing scope, reports two candidates besides this one:
`match_bt_backup` (`ladr/btm.c:892`) and its duplicate `unify_bt_backup`
(`ladr/btu.c:990`). Both are safe: they do `bt1 = bt1->parent` *before* freeing
the sibling chain, so the test only ever reads the live parent. The detector
flags them because its alias tracking has no flow sensitivity.

That list is worth only as much as the detector, so it was calibrated against
the known answer first: an earlier version found 0 candidates and would have
missed this very bug, because the condition dereferences `f` while the body
frees `tmp`, two assignments away. The version used here is self-tested — it is
run against the pre-fix `flatterm.c` and must report line 152 before its output
on the current tree is trusted.

### T4 — the negation of a quantified formula was silently dropped

The most serious defect in this document: `tptp_to_ladr` translated some
problems into the *opposite* of what they said, with no diagnostic.

```
TPTP in:   fof(ax4, axiom, ~ ! [U] : (sorti2(U) => ...commutative...) ).
LADR out:         sorti2(U) -> (all V (sorti2(V) -> op2(U,V) = op2(V,U)))
```

The input says the operation is **not** commutative. The output asserts that it
**is**. Measured across the five forms that matter:

| TPTP | should be | was emitted |
| --- | --- | --- |
| `~ ! [X] : p(X)` | `-(all X p(X))` | `p(X)` |
| `~ ? [X] : p(X)` | `-(exists X p(X))` | `p(X)` |
| `~ ! [X] : ~ p(X)` | `-(all X -p(X))` | `-p(X)` |
| `! [X] : ~ ? [Y] : q(X,Y)` | `(all X -(exists Y q(X,Y)))` | `(all X q(X,Y))` |
| `~ ( ! [X] : p(X) )` | `-(all X p(X))` | correct — parentheses saved it |

**Mechanism.** TPTP lets `~` apply to a quantified formula without
parentheses. `ladr/tptp_trans.c` declares `~` at precedence 410 and `:` at
450, so `~` binds tighter and the parser produces

```
:( ~(!([X])) , p(X) )        instead of    ~( :( !([X]), p(X) ) )
```

— the negation attached to the quantifier *prefix*. The converter's `:` branch,
whose comment reads `/* Assume it's well formed (don't check for errors). */`,
then called `listterm_reverse()` on `!([X])`, which is not a list, received the
empty list, never entered its variable loop, and returned the body. Both the
negation and the quantifier vanished. The same branch also chose `exists` for
any quantifier that was not `!`, without checking.

**Why precedence alone cannot fix it.** Raising `~` above `:` does make
`~ ! [X] : p(X)` parse correctly — and immediately breaks `! [X] : ~ p(X)`,
because `:` can then no longer take a `~`-formula as its right operand. The
failures swap; the count does not change. TPTP's grammar permits both nestings
because `~ <unitary>` and `! [X] : <unitary>` recurse through the same
nonterminal, which one precedence per operator cannot express. Setting them
equal (both 450) breaks the parser outright. The Changelog records McCune
trying twice, with `/* changed by McCune */` still on the line and an entry
reading *"The Feb 14 precedence change (~) was no good."*

**Fix** — in the converter, not the parse table: peel any `~` the parser bound
to the quantifier prefix, build the quantified formula, then re-apply the
negations outside it. The `:` branch now also rejects a quantifier that is
neither `!` nor `?` instead of guessing. All 15 cases in the matrix pass with
the precedence left exactly as McCune set it.

**How it surfaced.** As a false accusation against Mace4. The TPTP capability
sweep flagged dozens of problems where Mace4 reported a model for a problem
TPTP declares a `Theorem` — which would be a soundness bug in the model
finder. It was not: the translator had inverted an axiom, and Mace4 was
correctly finding a model of a different problem. On `ALG029+1`, the stock
build reports a model; with the negation preserved, no model is found, as must
be the case. **Every apparent soundness violation in that sweep traced back to
this defect, and none to Mace4 or Prover9.**

### T5 — the translator emitted files its own prover cannot read

TPTP quotes atoms with `'`, LADR with `"`. A TPTP formula name may be a
single-quoted atom, and `tptp_input_to_ladr_formula` copied it straight into
`label(...)`:

```
fof('EQ-var', axiom, ...)   ->   ... # label('EQ-var') # label(axiom).
```

LADR's reader then rejects the file the translator just wrote, with
`Fatal error: sread_term error` pointing at the label. 382 of the 17,912
first-order problems carry names of this form and every one of them failed at
the prover rather than the translator, so nothing in the translation output
hinted at the cause. The `-q` option does not help: it remaps function and
predicate symbols, not formula names.

Fixed by re-quoting the name in LADR's style when it arrives single-quoted,
leaving names that are already acceptable untouched.

### T3 — and then the buffers stopped being buffers

Sizing from `PATH_MAX` fixed the truncation but introduced a cost that only
showed up when measured: a 4953-byte array in each of five functions' frames.
In four of them that is merely wasteful. In the fifth it is worse —
`read_tptp_input` is **recursive**, following TPTP `include()` directives, so
the array was paid for per nesting level. Its frame went from about 200 bytes
to 5016, and a circular `include()` therefore exhausted the stack after 1651
levels where the unfixed build had managed 40262.

Both numbers are crashes, so the unbounded `include()` recursion is a
pre-existing defect and not something the buffer change caused. But cutting the
available depth 24-fold to gain a message size is a bad trade, and the right
answer is not to pick a better constant — it is not to have the array:

```c
/* ladr/fatal.c */
void fatal_file_not_found(char *context, char *filename)
{
  int n = snprintf(NULL, 0, "%s%sfile %s not found", ctx, sep, filename);
  ...
  Fatal_message = malloc((size_t) n + 1);
```

`snprintf` with a null destination and zero size returns the length the whole
message *would* need, so the message is measured and then allocated to fit. The
filename comes from input or `argv`, and nothing now bounds how much of it is
reported: a 20000-character name — four times `FILE_MESSAGE_SIZE` — comes back
in full. The five call sites are each one line, with no buffer at all, and
`read_tptp_input`'s frame is **40 bytes**. A circular include now reaches
**87255** levels, more than twice the unfixed build's depth, because the
original `char s[100]` is gone from the frame too.

Three details that each took a check rather than an assumption:

- **The allocator is plain `malloc`, not the LADR one.** `palloc()` calls
  `fatal_error()` when it cannot allocate, which from inside a fatal-error path
  would recurse.
- **`FILE_MESSAGE_SIZE` survives as the fallback** if `malloc` returns NULL —
  we may be here precisely because memory ran out. That path truncates, which
  is the correct trade when the alternative is saying nothing at all. It is
  also why the `PATH_MAX` + 20% derivation is still the one that governs the
  worst case.
- **The message pointer is file-scope, not local.** `fatal_error()` does not
  return, so the message cannot be freed. Held in a local, every run that
  reports a missing file would look like a leak to a leak checker; reachable
  from a file-scope pointer, it is not one. Verified: valgrind
  `--leak-check=full --show-leak-kinds=all` on that exact path reports
  **0 definitely, indirectly and possibly lost**, with the message among
  "still reachable" — which is what a pointer deliberately held to `exit()`
  should look like.

The messages themselves are byte-for-byte what they were, which the regression
tests check by matching the full text.

The five sites were the ones that format a filename. Buffers elsewhere in the
tree are sized by their own content, and the
`-Wformat-truncation=2 -Wformat-overflow=2` rebuild still reports nothing.

---

## Undefined behaviour in Prover9's given-clause selection

### U1 — signed integer overflow on **every default prover9 run**
`provers.src/giv_select.c:152,156`

```c
High.cycle_size += n;
```

`input_sos_first` defaults to `TRUE`, and `selector_rules_from_options` emits
its rule with `part = INT_MAX` (meaning "drain this selector first"). The next
rule's `n` is then added to `INT_MAX`:

```
giv_select.c:152:23: runtime error: signed integer overflow:
                     2147483647 + 1 cannot be represented in type 'int'
  #0 init_giv_select  provers.src/giv_select.c:152
  #1 init_search      provers.src/search.c:2144
```

This fired in **135 of the 168 corpus files** and in all 28 fuzzing findings —
i.e. essentially every run that uses the default selection rules. The
`cycle_size` field turns out to be **write-only** (nothing ever reads
`High.cycle_size`/`Low.cycle_size`), so today the only consequence is the
undefined behaviour itself; a `-ftrapv` or UBSan build aborts.

### U2 — the same overflow where the value *is* used
`provers.src/giv_select.c:105-115`

```c
int sum = 0;
for (p = s->selectors; p; p = p->next)
  if (avl_size(gs->idx) > 0)
    sum += gs->part;              /* gs->part can be INT_MAX */
```

`current_cycle_size()` feeds `iterations_to_selection()` at lines 409 and 481.
When an `INT_MAX`-part selector and any other selector in the same group both
hold clauses, the sum wraps **negative**, `iterations_to_selection` returns a
negative estimate, and `sos_keep2` / `worst_clause` then delete or displace the
wrong clauses. Unlike U1, this changes search behaviour rather than only UB
status.

Two routes reach it:

- the **High** group, where `input_sos_first` (default on) always contributes a
  part of `INT_MAX`; `current_cycle_size(&High)` is reached through
  `worst_clause`'s fallback to the High group;
- the **Low** group, if `hints_part` is set explicitly to something large but
  not exactly `INT_MAX` (`assign(hints_part, 2147483646)`), since that routes
  the hints rule into Low with that part value.

Both additionally need a search big enough to reach the `sos_limit` path, which
is why the corpus exercised U1 on nearly every file but never U2.

With the fix, a saturated `INT_MAX` cycle size is also the semantically right
answer: when a selector with part `INT_MAX` holds clauses, nothing in the other
selectors will ever be reached, so "effectively infinite iterations to
selection" is correct — whereas the wrapped negative value claimed those
clauses were about to be selected.

**Fix for U1 and U2** — one saturating helper, since `INT_MAX` is being used as
"infinity" and capping preserves exactly that meaning:

```c
static int add_parts(int a, int b)
{
  return (a > INT_MAX - b ? INT_MAX : a + b);
}
```

### U3 — `double`→`int` conversion defeats `sos_limit`
`provers.src/giv_select.c:460-462`

```c
int iters;
iters = least_iters_to_selection(c, &Low, NULL);   /* returns double */
...
if (iters < sos_limit / keep_factor)
  keep = TRUE;
```

`least_iters_to_selection` returns a `double` computed as
`n * (cycle_size/part) / (occurrences/sos_size)`, which readily exceeds
`INT_MAX`. Converting an out-of-range `double` to `int` is undefined behaviour;
on x86 it yields `INT_MIN`, which makes the comparison true and **keeps a
clause that should have been deleted** — silently disabling `sos_limit`
(default 20000, active by default) exactly for the clauses it is meant to catch.

**Fix** — keep `iters` a `double` and compare in `double`. The two `printf`s
use `%.0f`, so output is unchanged for in-range values.

---

## Option-dependency defects (Prover9)

These three were found while building a deterministic A/B harness: the obvious
way to neutralise a problem's time limit, `assign(max_minutes, -1)`, kills
prover9 outright. All three come from one line.

`ladr/options.c:713`

```c
int dval = p->multiply ? p->val * val : p->val;
...
assign_parm(p->id, dval, echo);     /* range-checks dval and calls fatal_error */
```

A parm can *derive* another parm's value (`search.c:282-284`,
`search.c:302-306`); `max_minutes` sets `max_seconds` with multiplier 60,
`max_hours` with 3600, `max_days` with 86400, and `pick_given_ratio` sets
`weight_part` with multiplier 1. The derived value is then pushed through the
normal range check, which calls `fatal_error` on failure — so a *derived*
value that lands out of range aborts the program.

### N1 — `-1` ("no limit") aborts the prover
`-1` is the documented no-limit value for `max_minutes`/`max_hours`/`max_days`
(it is also their default), but scaling it produces a value below
`max_seconds`'s minimum of `-1`:

```
$ prover9 -f <(echo 'assign(max_minutes,-1).') -f problem.in
assign_parm: parm max_seconds, value -60 out of range [-1..2147483647].
Fatal error:  assign_parm
```

`max_hours` gives `-3600`, `max_days` gives `-86400`. All three abort.

### N2 — signed overflow in the multiplier
`p->val * val` is plain `int` arithmetic:

| assigned | derived `max_seconds` |
| --- | --- |
| `assign(max_minutes, 50000000)` | `-1294967296` |
| `assign(max_hours, 1000000)` | `-694967296` |

Undefined behaviour, and the wrapped negative then aborts on the range check.

### N3 — a sentinel derived into a narrower range aborts
`pick_given_ratio` has minimum `-1`, but the `weight_part` it derives has
minimum `0`:

```
$ prover9 -f <(echo 'assign(pick_given_ratio,-1).') -f problem.in
assign_parm: parm weight_part, value -1 out of range [0..2147483647].
Fatal error:  assign_parm
```

**Fix for N1–N3** — one change: leave a negative sentinel unscaled, saturate
instead of overflowing, and clamp the derived value into the dependent parm's
range rather than aborting. A value the user assigns *directly* is still
range-checked and still errors, as before — only derived values are clamped.

| input | before | after |
| --- | --- | --- |
| `max_minutes, -1` | fatal | `max_seconds = -1` |
| `max_hours, -1` | fatal | `max_seconds = -1` |
| `max_days, -1` | fatal | `max_seconds = -1` |
| `max_minutes, 50000000` | fatal (overflow) | `max_seconds = INT_MAX` |
| `max_hours, 1000000` | fatal (overflow) | `max_seconds = INT_MAX` |
| `pick_given_ratio, -1` | fatal | `weight_part = 0` |
| `max_minutes, 2` | `max_seconds = 120` | `max_seconds = 120` |
| `max_hours, 3` | `max_seconds = 10800` | `max_seconds = 10800` |
| `pick_given_ratio, 4` | `weight_part = 4` | `weight_part = 4` |

The last three show that in-range derivations are untouched. Mace4 has no
`max_minutes`/`max_hours`/`max_days`, so it is unaffected.

## Logic defects that currently work only by accident

### S1 — spurious outer loop in Mace4's unary-function printer
`mace4.src/print.c:107`

```c
int i;
for (i = 0; i < n; i++) {        /* outer loop */
  printf("\n %s :\n", name);
  for (i = 0; i < n; i++)        /* reuses i */
    printf(s1, i);
  ...
}
```

Three inner loops reuse the outer loop's counter and leave it at `n`, so the
outer body executes exactly once — which is what the output needs. The printed
table is correct *only* because of the clobbering; tidying the shadowing
without noticing the outer loop would print the table `n` times.

**Fix** — delete the outer loop, matching the `arity == 2` branch below it.
Byte-identical output (domain size is always ≥ 2).

### S2 — bitwise `|` where `||` was intended
`ladr/clash.c:283`, `ladr/paramod.c:347`

```c
else if (!p->clashable | p->clashed)
if ((!VARIABLE(into) | Para_into_vars) && basic_check(into))
```

`!x` yields 0 or 1 and the right operands are booleans, so the truth value is
the same and there are no side effects to short-circuit — no behavioural bug,
but it is what `-Wparentheses` is warning about.

**Fix** — `||`.

### S3 — `BOOL` holding a non-`BOOL` enum
`ladr/flatdemod.c:105`

```c
BOOL status = GO;      /* BOOL is enum {FALSE=0,TRUE=1}; GO is from enum {GO,BACKTRACK} */
```

Works only because `GO`/`BACKTRACK` happen to be 0/1, the same values as
`FALSE`/`TRUE`. **Fix** — declare `status` as `int`.

### S4 — pointer truncated to `int` in a debug printer
`ladr/avltree.c:658` — `printf("%4d\n", (int) p->item)` discards the top 32
bits of a pointer on LP64. Only reachable from the `p_avl` debug helper.
**Fix** — `%4p`.

---

### S5 — arithmetic on `void *` in the allocator

Every LADR allocation goes through `palloc()`, which carves chunks out of a
malloc'd block using two `void *` cursors:

```c
static void *Block = NULL, *Block_pos = NULL;
...
if (Block==NULL || Block + malloc_bytes - Block_pos < n)   /* line 58 */
...
Block_pos += n;                                            /* line 78 */
```

Arithmetic on `void *` is a GCC extension — it treats `void` as having size 1
— and not standard C; a conforming compiler may reject it, and MSVC does.
Declaring the cursors `char *` gives identical byte arithmetic portably. The
comparison on line 58 gets an explicit `(size_t)` cast so it stays an unsigned
comparison now that the subtraction yields `ptrdiff_t`.

Checked rather than assumed, in both directions: under `-std=c17 -pedantic`
the old file emits three `-Wpointer-arith` diagnostics (lines 58 ×2 and 78) and
the new one emits none, and a `-Wpointer-arith` sweep of `ladr`,
`provers.src`, `mace4.src` and `apps.src` now reports **0** sites tree-wide.
This one was pointed out by `jamiewannenburg/ladr`, which fixed it
independently while porting LADR to MSVC.

## Performance

### PF1 — `String_buf` appending is quadratic
`ladr/strbuf.c`

`sb_append_char` walked the entire chunk list **on every character** to find
the tail, so building an *n*-character buffer cost O(n²); `sb_append` did the
same walk per call. `sb_cat_copy` indexed its source with `sb_char(sb2, i)`,
which also rescans per character — the author's own comment there reads
`/* Note that this is inefficient if there are many chunks in either sb. */`.
This is on the proof-output and comment-handling paths.

**Fix** — cache the tail chunk in `struct string_buf` (the struct is private to
`strbuf.c`, and `get_cmem` zeroes it, so the new field needs no explicit
initialisation), and walk the chunks directly in `sb_cat_copy`, still stopping
at an embedded NUL as the original did.

Measured with `test.src/strbuf_test.c` (new, run with `make -C test.src check`),
400 000 `sb_append_char` calls:

| | time |
| --- | --- |
| before | 3.29 s |
| after | 0.00 s |

All 14 semantic assertions in that test pass identically before and after.

The one cost: the extra pointer grows `struct string_buf` from 16 to 24 bytes.
That shows up in Prover9's memory-usage report as `string_buf ( 24)` instead of
`string_buf ( 16)`, and it is the reason for the only output differences the
corpus A/B diff turned up (see below).

---

## The installed helper scripts (`utilities/` → `bin/`)

`make all` finishes with `/bin/cp -p utilities/* bin`, so these ship as
user-facing commands. Every one of them was broken on this machine.

### X1 — `#!/usr/bin/python` no longer exists
`utilities/attack`, `utilities/looper`, `utilities/gvizify`,
`utilities/prover9-mace4`, `rev.py`

Current distributions ship no `/usr/bin/python` at all (only `python3`), so all
five fail immediately with "no such file or directory" regardless of their
contents. **Fix** — `#!/usr/bin/env python3`.

### X2 — Python 2 `print` statements
`utilities/attack`, `utilities/looper`, `utilities/gvizify`

Even with a working interpreter, three of them are Python 2 and die at parse
time:

```
  File "utilities/attack", line 36
    print "Starting job:", command_and_args
SyntaxError: Missing parentheses in call to 'print'. Did you mean print(...)?
```

**Fix** — converted the 20 `print` statements to `print()` calls. All were the
plain `print EXPR` form (no trailing comma, no `print >>f`), so the conversion
is mechanical and output-preserving.

### X3 — `subprocess` pipes are bytes in Python 3
`utilities/attack:39`, `utilities/looper:36`

```c
p1 = Popen(command_and_args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
(out, err) = p1.communicate(stdin_string)     /* stdin_string is a str */
```

Under Python 3 this raises `TypeError: memoryview: a bytes-like object is
required`, and the `re` matching both scripts then do on `out` would fail too.
**Fix** — `text=True` on the `Popen`, which keeps `communicate(str)` and all
the downstream string handling working.

### X4 — invalid regex escapes
`utilities/attack:87` (`'\s*%'`), `rev.py:12` (`'\S'`)

`SyntaxWarning: invalid escape sequence` today, a hard error in a future
Python. **Fix** — raw strings.

### X5 — `gvizify` is shipped without the execute bit
`utilities/gvizify`

It is mode `100644` in the tarball while every other script is `100755`, so
after installation it cannot be run as a command:

```
$ gvizify proof.xml
bash: .../bin/gvizify: Permission denied
```

**Fix** — `chmod +x`. (`utilities/proof3fo.xsl` is correctly non-executable; it
is data, not a script.)

All four utilities were then run end to end against freshly built binaries:
`looper mace4 assumptions` and `looper prover9 goals` drive searches and report
results, `attack` filters candidates and writes its interpretation file,
`gvizify` turns `prooftrans xml` output into valid graphviz `dot`, and
`prover9-mace4 -f file` races the two provers and returns the winner's output.

### Known limitation, deliberately left alone
`utilities/prover9-mace4` reports failure as soon as the *first* of the two
processes exits, even when that one failed and the other would have succeeded:

```
$ prover9-mace4 -f satisfiable.in
failure, Prover9 ended by sos empty       # Mace4 was about to find the model
```

The script's own header documents this ("the one that finishes first might have
failed ... We'll miss the solution in that case") and adds "This script might
not be very useful". That is a design decision by the author rather than a
defect, so it is reported, not changed. The fix, if wanted, is to keep waiting
on the other process when the first one exits without success.

## Latent defect left documented but unfixed

### L1 — `demod1` discards a term that `demod1_recurse` may have freed
`ladr/demod.c:376-384`

```c
Term result;
result = demod1_recurse(lit->atom, lit->atom, demodulator, direction,
                        ipos, lex_order_vars);
```

`demod1_recurse` can `zap_term(t)` and return a replacement (lines 344-345);
every recursive call therefore assigns the result back (`ARG(t,i) = ...`).
`demod1` does not, so a rewrite at the top of the atom would leave `lit->atom`
dangling and leak the contractum. The function's own documentation says it
should "return the rewritten term", and it returns `void`.

`demod1` has **no callers anywhere in the tree** — it is dead code exported in
`demod.h`. Changing it would alter the behaviour of an untested function with no
way to validate the result, so it is reported rather than patched. The fix, if
it is ever called, is `lit->atom = demod1_recurse(...)`.

---

## Behaviour preservation

The important question for a theorem prover is whether the fixes changed any
*answers*. They did not. Verification, all against a **baseline** built from
pristine upstream plus only the build fixes B1–B3 (so the baseline compiles but
is otherwise untouched):

**1. Upstream's own smoke tests** — `make test1` / `test2` / `test3` pass before
and after.

These numbers are from the final tree (after the `snprintf` conversion); the
run before that conversion produced the identical counts on the identical
files, so the conversion changed no output anywhere.

**2. Prover9, output diff over the whole corpus.** 168 inputs, run through both
binaries, with timings, PIDs, dates, host names and memory figures normalised
away. Searches were bounded by `max_given=300` rather than by wall-clock time,
because a time-bounded run does a different amount of work on every invocation
and would make the diff meaningless:

```
## prover9: identical=165 differing=2 timeout=1   (ran-a-search=135, input-rejected=32)
```

The 32 "input-rejected" files are Mace4 puzzles and data files that Prover9
declines identically on both sides. The 2 differences are
`fof/ALG033+1.in` and `fof/ALG043+1.in`, both of which terminate by exhausting
`max_megs`; they differ only in *where* the 400 MB ceiling falls, because
`struct string_buf` is 8 bytes larger after PF1:

```
< string_buf (  16)          4903       4903          0
> string_buf (  24)          4903       4903          0
< formula (  48)         10475685    6983463    3492222
> formula (  48)         10474839    6982899    3491940
```

Both reach the same conclusion (`exit (max_megs)`) and neither finds a proof.

**3. Mace4, output diff over Mace4-appropriate inputs.** The shipped
`mace4.examples` / `apps.examples` plus the upstream `mace4-misc` set — 18
problems — each at domain sizes 2, 3 and 4 with `max_models=-1`, so every run
enumerates the same finite space. 18 × 3 = 54 runs:

```
## mace4: identical=49 differing=3 timeout=2   (ran-a-search=49, input-rejected=3)
```

The 3 differences are all `mace4.examples/group2.in` — the only input in the
set that both reaches and prints the `set(verbose)` memory report — and consist
solely of the same `string_buf ( 16)` → `( 24)` line plus the extra 24-byte
free-list bucket that the larger struct adds to the report. Every model table
and count is identical. The 2 timeouts are `steam.in` on both sides.

**4. Mace4 soundness.** `test.src/mace4_soundness.sh` has Mace4 enumerate all
models of four problems (quasigroups, groups, a strict partial order, and a
mixed function/predicate problem) at domain sizes 2–4, then has `clausetester`
independently evaluate every input clause in every model:

```
SOUNDNESS OK: 271 models verified, every one satisfies every input clause
```

Baseline and fixed produce **the same 271 models**, with identical per-size
counts.

**5. The apps.** The first three checks cover `prover9` and `mace4`; the
programs built on top of them were checked separately, because the `snprintf`
work touched the shared term- and justification-printing paths
(`ladr/term.c`, `ladr/parse.c`, `ladr/just.c`, `ladr/symbols.c`).
`prooftrans` and `directproof` were run in all seven output formats over a
1-proof and a 3-proof input, including the `expand` and `tagged` formats that
exercise the `jmap2` clause-id labels whose buffer shrank from `char[21]` to
`MAX_JMAP_SYMBOL_SIZE`:

| | result |
| --- | --- |
| `hints`, `ivy` (`prooftrans`) | byte-identical |
| every other format, both tools | one blank line per proof removed, nothing else |

That blank line is the intended D1 side effect described above, not a
regression. **This is also a gap in the earlier verification worth naming: the
corpus A/B diff ran only `prover9` and `mace4`, so it could not have surfaced
this.**

`interpformat`, `isofilter` and `clausetester` are exercised by `make test2`,
`make test3` and the soundness check above.

**6. Buffer sizing, checked by the compiler and at run time.** Rebuilding with
`-Wformat-truncation=2 -Wformat-overflow=2 -Wstringop-overflow=4 -Wformat=2`
produces no truncation or overflow warnings, so every `snprintf` destination is
provably large enough for the widest output its format can produce. A fresh
ASan + UBSan build was then swept over the whole corpus again, 168 files
through each program — **336 runs, 0 findings**: no ASan report, no UBSan
report, no signal, no unexpected exit status.

Establishing that this zero is not vacuous took three separate checks, because
an earlier version of this very claim *was* vacuous and had to be retracted
(see the ⚠️ Correction in "How the survey was done"):

1. **The tree was built from sources only** — `git archive HEAD` into an empty
   directory, then `make clean` — never by copying a built tree. That copying
   is what silently defeated the first three sweeps, because `make` saw the
   library objects as up to date and never recompiled them with the flags.
2. **Instrumentation was verified per object, not per binary.** 137 of 137
   compile lines carry `-fsanitize`, and all 77 members of `libladr.a`
   reference `__asan`. Checking the binary for `__asan_init` — which is what
   the retracted claim relied on — proves only that the runtime was linked,
   and says nothing about which objects were compiled with the flags.
3. **The positive control covers the code the claim is about.** `ladr/dollar.c`
   was reverted to its pre-fix state, with nothing else changed, and the tree
   rebuilt: that binary reports `ERROR: AddressSanitizer: heap-buffer-overflow`
   on the M7 input while the shipped binary is clean on it. The earlier control
   used a `giv_select.c` defect in `provers.src`, one of the directories that
   *had* been instrumented, so it passed while the library went unchecked.

Run against the instrumented build, the regression suite reports 27 of 27
passing, the M7 test included; on a plain build that test reports SKIP.

The programs built on top of the libraries were also checked directly, since
the corpus sweep drives only `prover9` and `mace4`: `prooftrans` and
`directproof` in all seven output formats under ASan + UBSan **with leak
detection enabled** are clean in all 14 combinations, the new `-label`
allocation leaks nothing and reproduces a 300-character label intact, and
Mace4's model printer is clean at domain sizes 4, 10, 11 and 12 — the sizes
either side of the `n <= 10` formatting switch in `print.c` that S1 and F2
touched.

**7. Regression suite.** `test.src/regress.sh` — 35 checks. One of them, the M7
heap overflow, is observable only under a sanitizer, so on a plain build it
reports SKIP and 34 run: all 34 pass on the fixed tree and **27 of them fail on
the baseline**. Against the instrumented build all 35 run and all 35 pass. The 7
that pass on both sides are the deliberate "must not regress" controls (a
complete proof still transforms correctly, in-range parm derivations still
produce their old values, a default Prover9 run still finds its proof, a TPTP
problem still translates and proves and translates back). The two
D2 tests were also run against a
build that carries every fix *except* D2, where they are the only two failures —
so they test what they claim to and nothing else.

### Fixes that intentionally *can* change results

Only where the old code's behaviour was undefined or absent:

- **U2** — a wrapped-negative cycle size produced wrong clause-retention
  decisions; it now saturates.
- **U3** — `sos_limit` now actually deletes the clauses it was meant to, instead
  of keeping them because a `double`→`int` conversion overflowed to `INT_MIN`.
- **N1–N3** — option assignments that previously aborted the program now take
  effect.
- **D1** — truncated proof input now reports an error instead of hanging and
  then crashing.
- **PF1** — 8 bytes more per `String_buf`, visible only in the memory report and
  at a `max_megs` boundary, as item 2 above shows.

Everything else is a build fix, a buffer-size change on an error-reporting
path, or a semantics-preserving rewrite.

---

## Removed: upstream's in-tree backups

The tarball ships 83 files that nothing builds. They are version control done
by hand, from before the project was in a repository, and the naming makes the
intent plain:

| Where | What | Last touched |
| --- | --- | --- |
| `ladr/save/` | 44 files: whole old copies of `clause.c`, `commands.c`, `sos.c`, `detach.c`, `weight1.c`, `read_tptp_file.c`; plus `sos.c.old`, `detach.h.bak`, `sos.h~`, `cnf.c.alarm`, and a 2004 `detach.o` | 2004–2007 |
| `provers.src/save/` | 20 files, same pattern: `auto.c`, `picker.c`, `poptions.c`, `ploop4.c`, `prover9-simple.c` | 2004–2008 |
| `mace4.src/save/` | 2 files: `raw-cooked` and `raw-cooked~` | 2005 |
| `bob/` | 5 files: a contributor's patch directory | 2007–2008 |
| live source dirs | 12 loose `.c~`, `.h.bak`, `.c.work`, `.py~`, `.in~` files sitting among the real sources, including `memory.c~` and `tptp_trans.c~` | 2004–2009 |

Several files in the `save/` directories have no `.c` extension at all —
`lex_compare`, `printing`, `goals_to_denials`, `str_toupper`,
`interp_remove_skolems`, `fork_and_wait` — and each contains exactly one
function, under that function's own name, complete with its `/* DOCUMENTATION
*/` block. They are individual routines cut out of the live sources and kept
by hand in case they were wanted back.

Nothing was load-bearing, and each case was checked rather than assumed:

- No `Makefile`, script or test in the tree names any of them.
- Every `save/` file either duplicates a live source or holds a function that
  no longer exists. The `~`/`.bak`/`.work` files in the live directories are
  older copies of files beside them, and no `Makefile` names those either —
  `.c~` and `.c.work` do not match a `%.c` rule.
- `bob/` is Bob Veroff's working directory for the tagged-proof contribution,
  dated 2007-aug-20 in its own `ChangelogBV`. **That feature was merged**:
  `apps.src/prooftrans.c:277` still carries his `/* BV(2007-aug-20) */` marker,
  `ladr/ioutil.h` declares `sb_tagged_write_clause_jmap()`, and
  `prooftrans tagged` works. The directory is the superseded staging copy.
- `ladr/save/read_tptp_file.c` is the one with any claim to interest: an
  earlier in-library TPTP reader, replaced by `ladr/tptp_trans.c` plus the two
  translator programs, which are built and now tested.

Deleting them is reversible and loses nothing: the first commit in this
repository is the untouched tarball, so

```sh
git show 25b4505:ladr/save/sos.c          # any single file
git checkout 25b4505 -- ladr/save bob provers.src/save mace4.src/save
```

still produce the originals.

**Kept deliberately:** `ladr/Changelog.old` and `mace4.src/Changelog.old`. The
`.old` suffix makes them look like more of the same, but they are not backups
of anything — they are the only surviving record of LADR's early history, and
they end by pointing at the file that continues it ("Further changes are listed
in the LADR Changelog file (`../Changelog`)"). `ladr/Changelog.old` is also
where the TPTP relationship first appears: *"May 18, 2004. Soundness bug from
Geoff Sutcliffe. CNF transformation... Since this is a serious bug, a new
release will be made today."*
