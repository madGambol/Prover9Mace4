# LADR/apps.src

CC = gcc

# XFLAGS can be specified on the command line (see XFLAGS below)

CFLAGS = $(XFLAGS) -O -Wall

# libladr.a contains one C++ translation unit (i2accum_cxx.cpp), so the C++
# standard library has to be linked wherever libladr is linked.  -lstdc++
# rather than linking with g++, so that CC and CFLAGS stay as they were.
LADRLIBS = -lstdc++
# CFLAGS = $(XFLAGS) -O6 -Wall
# CFLAGS = $(XFLAGS) -g  -O -Wall
CFLAGS = $(XFLAGS) -g  -O0 -Wall
# CFLAGS = $(XFLAGS) -pg -O -Wall
# CFLAGS = $(XFLAGS)  -Wall -pedantic

PROGRAMS = avltest strbuf_test memory_test arity_test varnum_test bool_test \
           multiset_test glist_test features_test discrim_test

all: ladr apps

ladr:
	make clean
	cd ../ladr && $(MAKE) libladr.a

clean:
	/bin/rm -f *.o

realclean:
	/bin/rm -f *.o $(PROGRAMS)

tags:
	etags *.c ../ladr/*.c

apps: $(PROGRAMS)

avltest: avltest.o
	$(CC) $(CFLAGS) -o avltest avltest.o ../ladr/libladr.a -lm $(LADRLIBS)

strbuf_test: strbuf_test.o
	$(CC) $(CFLAGS) -o strbuf_test strbuf_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks that a BOOL holds only TRUE or FALSE; microseconds.
bool_test: bool_test.o
	$(CC) $(CFLAGS) -o bool_test bool_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks that the discrimination index's key field is wide enough for a symbol
# number: it was a short, which truncates above 32,767 and makes the prover
# abort.  Needs ~40,000 symbols, so a second or two -- tier2.
discrim_test: discrim_test.o
	$(CC) $(CFLAGS) -o discrim_test discrim_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks the sparse feature-vector index against a brute-force dense scan, in
# both directions, on random data; under a second.
features_test: features_test.o
	$(CC) $(CFLAGS) -o features_test features_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks the 40 list functions that used to recurse once per element: the
# transcript must match what the recursive versions produced, byte for byte,
# and a 400,000-element list must not overflow the stack; under a second.
glist_test: glist_test.o
	$(CC) $(CFLAGS) -o glist_test glist_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks the symbol-multiset packages: that the hash accumulator agrees with
# the list implementation element for element and IN ORDER (precedence, and so
# every proof, depends on that order), and that neither is quadratic or
# recursive any more; under a second.
multiset_test: multiset_test.o
	$(CC) $(CFLAGS) -o multiset_test multiset_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks the named types around variables and symbols; microseconds.
varnum_test: varnum_test.o
	$(CC) $(CFLAGS) -o varnum_test varnum_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks the arity ceiling and that widening it cost no memory; milliseconds.
arity_test: arity_test.o
	$(CC) $(CFLAGS) -o arity_test arity_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Checks that the palloc() byte counter is wide enough; needs ~2.2GB.
memory_test: memory_test.o
	$(CC) $(CFLAGS) -o memory_test memory_test.o ../ladr/libladr.a -lm $(LADRLIBS)

# Run the regression tests (each exits nonzero on failure).
#   strbuf_test        - String_buf semantics + the non-quadratic append
#   regress.sh         - the defects listed in ../FIXES.md
#   mace4_soundness.sh - every model Mace4 reports really does satisfy the
#                        input clauses (checked independently by clausetester)
# Each accepts a bin directory, e.g.  ./regress.sh /path/to/other/bin
check: strbuf_test memory_test arity_test varnum_test bool_test multiset_test glist_test
	./arity_test
	./varnum_test
	./bool_test
	./multiset_test
	./glist_test
	./features_test
	./arity_check_test.sh
	./strbuf_test
	./memory_test
	./discrim_test
	./regress.sh
	./mace4_soundness.sh

# Two tiers, split where the cost actually jumps.
#
#   tier1  about 2 seconds.  The demonstrations and the semantic translation
#          matrix.  Fast enough to run on every edit.
#   tier2  about 2 1/2 minutes.  Everything: the 41 regression tests, the
#          String_buf unit test, 271 re-verified models, and test1/2/3.
#
# Deliberately NOT a sampled tier2: the whole of tier2 costs under three
# minutes, so sampling it would trade a real chance of missing a regression
# for about two minutes.
#
#   tier3  the memory-safety sweeps: AddressSanitizer + UBSan over the whole
#          corpus, LADR's own poisoned allocator against an unpoisoned
#          control, and a valgrind memcheck sample.  Tens of minutes.  Needs a
#          corpus, so:  make tier3 CORPUS=/path/to/list-of-inputs
#
# tier3 used to be three procedures described in a comment rather than three
# scripts, and the result was predictable: it was skipped for five consecutive
# merges to rc and master, during work that rewrote forty list functions as
# manual pointer surgery and changed the layout of struct di_tree.  A gate
# that depends on remembering to run it is not a gate, so it is a target now.
#
# determinism.sh had the same problem in a smaller way: it was written, it
# worked, and no target invoked it, so it ran once and then never again.  It
# is in tier2 now, with a generated default set.  The rule is worth stating
# plainly: a script in this directory that no target invokes is documentation,
# not a check.  The whole audit is
#
#   for s in *.sh; do grep -q "$s" Makefile || echo "unwired: $s"; done
#
# (normalize_output.sh is the one legitimate exception -- it is a filter the
# other scripts call, not a check of its own.)
#
# The three are not interchangeable.  ASan finds out-of-bounds access and
# undefined behaviour; valgrind finds reads of never-written memory and says
# where the value came from; and the poisoned allocator finds stale reads
# through LADR's avail lists, which NEITHER of the others can see, because
# free_mem() pushes a cell onto a free list instead of returning it to the
# system and both tools therefore see a live arena.  That is how the
# zap_flatterm use-after-free was caught after a clean ASan sweep.
#
# Still not make targets, because they need hours and their own data: the TPTP
# oracle sweep and the capability runs.  Those belong at a release.
#
# tier2 also accepts BASELINE=<bin dir> and then checks that the defect tests
# still FAIL there.  A test that passes against the unfixed code is not a
# regression test, and that is the single mistake this suite has made most
# often, so it is worth asserting rather than remembering.

tier1: arity_test varnum_test bool_test multiset_test glist_test
	./arity_test
	./varnum_test
	./bool_test
	./multiset_test
	./glist_test
	./features_test
	./arity_check_test.sh
	./demos/run.sh
	./tptp_semantics.sh

tier2: strbuf_test memory_test discrim_test tier1
	./strbuf_test
	./memory_test
	./regress.sh
	./mace4_soundness.sh
	./determinism.sh 3
	cd .. && $(MAKE) test1 && $(MAKE) test2 && $(MAKE) test3
	@if [ -n "$(BASELINE)" ]; then \
	  echo "--- checking the defect tests still fail against $(BASELINE) ---"; \
	  ./regress.sh "$(BASELINE)" > /tmp/ladr-baseline-run.txt 2>&1; \
	  n=$$(grep -c '  FAIL  ' /tmp/ladr-baseline-run.txt); \
	  echo "    $$n of 41 fail against the baseline"; \
	  if [ "$$n" -lt 30 ]; then \
	    echo "    TOO FEW: tests that pass on the unfixed code prove nothing"; \
	    exit 1; \
	  fi; \
	fi

tier3:
	@if [ -z "$(CORPUS)" ]; then \
	  echo "tier3 needs a corpus:  make tier3 CORPUS=/path/to/list-of-inputs"; \
	  exit 2; \
	fi
	./sanitizer_sweep.sh "$(CORPUS)"
	./poison_sweep.sh "$(CORPUS)"
	./valgrind_sweep.sh "$(CORPUS)"

demos:
	./demos/run.sh

tptp_test: tptp_test.o
	$(CC) $(CFLAGS) -o tptp_test tptp_test.o ../ladr/libladr.a -lm $(LADRLIBS)

