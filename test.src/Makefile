# LADR/apps.src

CC = gcc

# XFLAGS can be specified on the command line (see XFLAGS below)

CFLAGS = $(XFLAGS) -O -Wall
# CFLAGS = $(XFLAGS) -O6 -Wall
# CFLAGS = $(XFLAGS) -g  -O -Wall
CFLAGS = $(XFLAGS) -g  -O0 -Wall
# CFLAGS = $(XFLAGS) -pg -O -Wall
# CFLAGS = $(XFLAGS)  -Wall -pedantic

PROGRAMS = avltest strbuf_test memory_test arity_test varnum_test bool_test \
           multiset_test glist_test

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
	$(CC) $(CFLAGS) -o avltest avltest.o ../ladr/libladr.a -lm

strbuf_test: strbuf_test.o
	$(CC) $(CFLAGS) -o strbuf_test strbuf_test.o ../ladr/libladr.a -lm

# Checks that a BOOL holds only TRUE or FALSE; microseconds.
bool_test: bool_test.o
	$(CC) $(CFLAGS) -o bool_test bool_test.o ../ladr/libladr.a -lm

# Checks the 40 list functions that used to recurse once per element: the
# transcript must match what the recursive versions produced, byte for byte,
# and a 400,000-element list must not overflow the stack; under a second.
glist_test: glist_test.o
	$(CC) $(CFLAGS) -o glist_test glist_test.o ../ladr/libladr.a -lm

# Checks the symbol-multiset packages: that the hash accumulator agrees with
# the list implementation element for element and IN ORDER (precedence, and so
# every proof, depends on that order), and that neither is quadratic or
# recursive any more; under a second.
multiset_test: multiset_test.o
	$(CC) $(CFLAGS) -o multiset_test multiset_test.o ../ladr/libladr.a -lm

# Checks the named types around variables and symbols; microseconds.
varnum_test: varnum_test.o
	$(CC) $(CFLAGS) -o varnum_test varnum_test.o ../ladr/libladr.a -lm

# Checks the arity ceiling and that widening it cost no memory; milliseconds.
arity_test: arity_test.o
	$(CC) $(CFLAGS) -o arity_test arity_test.o ../ladr/libladr.a -lm

# Checks that the palloc() byte counter is wide enough; needs ~2.2GB.
memory_test: memory_test.o
	$(CC) $(CFLAGS) -o memory_test memory_test.o ../ladr/libladr.a -lm

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
	./strbuf_test
	./memory_test
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
# for about two minutes.  The expensive checks are a tier further out and are
# not make targets because they need their own corpora -- the sanitizer sweep
# (about 25 min), the valgrind sweep (about 40 min), and the TPTP oracle and
# capability runs (hours).  Those belong at the merge-to-rc and release gates.
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
	./demos/run.sh
	./tptp_semantics.sh

tier2: strbuf_test memory_test tier1
	./strbuf_test
	./memory_test
	./regress.sh
	./mace4_soundness.sh
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

demos:
	./demos/run.sh

tptp_test: tptp_test.o
	$(CC) $(CFLAGS) -o tptp_test tptp_test.o ../ladr/libladr.a -lm

