# LADR/apps.src

CC = gcc

# XFLAGS can be specified on the command line (see XFLAGS below)

CFLAGS = $(XFLAGS) -O -Wall
# CFLAGS = $(XFLAGS) -O6 -Wall
# CFLAGS = $(XFLAGS) -g  -O -Wall
CFLAGS = $(XFLAGS) -g  -O0 -Wall
# CFLAGS = $(XFLAGS) -pg -O -Wall
# CFLAGS = $(XFLAGS)  -Wall -pedantic

PROGRAMS = avltest strbuf_test

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

# Run the regression tests (each exits nonzero on failure).
#   strbuf_test        - String_buf semantics + the non-quadratic append
#   regress.sh         - the defects listed in ../FIXES.md
#   mace4_soundness.sh - every model Mace4 reports really does satisfy the
#                        input clauses (checked independently by clausetester)
# Each accepts a bin directory, e.g.  ./regress.sh /path/to/other/bin
check: strbuf_test
	./strbuf_test
	./regress.sh
	./mace4_soundness.sh

tptp_test: tptp_test.o
	$(CC) $(CFLAGS) -o tptp_test tptp_test.o ../ladr/libladr.a -lm

