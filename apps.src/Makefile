#  Copyright (C) 2006, 2007 William McCune
#
#  NOTICE ADDED IN THIS TREE: the LADR-2009-11A release shipped this
#  file without a licence notice.  What follows is the notice every
#  .c and .h file of that release carries, reproduced so that this
#  file is self-describing.  The terms are the release's own; this
#  is not a new grant, and McCune did not write these lines here.
#
#  This file is part of the LADR Deduction Library.
#
#  The LADR Deduction Library is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License,
#  version 2.
#
#  The LADR Deduction Library is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the LADR Deduction Library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

# CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena on
# 2026-10-10 through 2026-10-16, and is not as William McCune released it in
# LADR-2009-11A. What changed and why is in FIXES.md; this file's own diff
# against the unmodified release is
#
#     git diff 25b4505 -- apps.src/Makefile

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
# CFLAGS = $(XFLAGS) -g -O0 -Wall
# CFLAGS = $(XFLAGS) -pg -O -Wall
# CFLAGS = $(XFLAGS)  -Wall -pedantic

PROGRAMS = latfilter olfilter clausefilter idfilter renamer unfast clausetester rewriter isofilter0 isofilter isofilter2 dprofiles interpfilter upper-covers miniscope interpformat prooftrans mirror-flip perm3 sigtest directproof test_clause_eval test_complex complex gen_trc_defs

all: ladr apps install realclean

ladr:
	cd ../ladr && $(MAKE) libladr.a

clean:
	/bin/rm -f *.o

realclean:
	/bin/rm -f *.o $(PROGRAMS)

install:
	/bin/mkdir -p ../bin
	/bin/mv $(PROGRAMS) ../bin

tags:
	etags *.c ../ladr/*.c

apps: $(PROGRAMS)

test: test.o
	$(CC) $(CFLAGS) -o test test.o ../ladr/libladr.a -lm $(LADRLIBS)

interpformat: interpformat.o
	$(CC) $(CFLAGS) -o interpformat interpformat.o ../ladr/libladr.a -lm $(LADRLIBS)

prooftrans: prooftrans.o
	$(CC) $(CFLAGS) -o prooftrans prooftrans.o ../ladr/libladr.a -lm $(LADRLIBS)

directproof: directproof.o
	$(CC) $(CFLAGS) -o directproof directproof.o ../ladr/libladr.a -lm $(LADRLIBS)

test_clause_eval: test_clause_eval.o
	$(CC) $(CFLAGS) -o test_clause_eval test_clause_eval.o ../ladr/libladr.a -lm $(LADRLIBS)

test_complex: test_complex.o
	$(CC) $(CFLAGS) -o test_complex test_complex.o ../ladr/libladr.a -lm $(LADRLIBS)

complex: complex.o
	$(CC) $(CFLAGS) -o complex complex.o ../ladr/libladr.a -lm $(LADRLIBS)

latfilter: latfilter.o
	$(CC) $(CFLAGS) -o latfilter latfilter.o ../ladr/libladr.a -lm $(LADRLIBS)

olfilter: olfilter.o
	$(CC) $(CFLAGS) -o olfilter olfilter.o ../ladr/libladr.a -lm $(LADRLIBS)

unfast: unfast.o
	$(CC) $(CFLAGS) -o unfast unfast.o ../ladr/libladr.a -lm $(LADRLIBS)

upper-covers: upper-covers.o
	$(CC) $(CFLAGS) -o upper-covers upper-covers.o ../ladr/libladr.a -lm $(LADRLIBS)

miniscope: miniscope.o
	$(CC) $(CFLAGS) -o miniscope miniscope.o ../ladr/libladr.a -lm $(LADRLIBS)

isofilter0: isofilter0.o
	$(CC) $(CFLAGS) -o isofilter0 isofilter0.o ../ladr/libladr.a -lm $(LADRLIBS)

isofilter: isofilter.o
	$(CC) $(CFLAGS) -o isofilter isofilter.o ../ladr/libladr.a -lm $(LADRLIBS)

isofilter2: isofilter2.o
	$(CC) $(CFLAGS) -o isofilter2 isofilter2.o ../ladr/libladr.a -lm $(LADRLIBS)

dprofiles: dprofiles.o
	$(CC) $(CFLAGS) -o dprofiles dprofiles.o ../ladr/libladr.a -lm $(LADRLIBS)

renamer: renamer.o
	$(CC) $(CFLAGS) -o renamer renamer.o ../ladr/libladr.a -lm $(LADRLIBS)

rewriter: rewriter.o
	$(CC) $(CFLAGS) -o rewriter rewriter.o ../ladr/libladr.a -lm $(LADRLIBS)

idfilter: idfilter.o
	$(CC) $(CFLAGS) -o idfilter idfilter.o ../ladr/libladr.a -lm $(LADRLIBS)

clausefilter: clausefilter.o
	$(CC) $(CFLAGS) -o clausefilter clausefilter.o ../ladr/libladr.a -lm $(LADRLIBS)

interpfilter: interpfilter.o
	$(CC) $(CFLAGS) -o interpfilter interpfilter.o ../ladr/libladr.a -lm $(LADRLIBS)

clausetester: clausetester.o
	$(CC) $(CFLAGS) -o clausetester clausetester.o ../ladr/libladr.a -lm $(LADRLIBS)

mirror-flip: mirror-flip.o
	$(CC) $(CFLAGS) -o mirror-flip mirror-flip.o ../ladr/libladr.a -lm $(LADRLIBS)

perm3: perm3.o
	$(CC) $(CFLAGS) -o perm3 perm3.o ../ladr/libladr.a -lm $(LADRLIBS)

sigtest: sigtest.o
	$(CC) $(CFLAGS) -o sigtest sigtest.o ../ladr/libladr.a -lm $(LADRLIBS)

rewriter2: rewriter2.o
	$(CC) $(CFLAGS) -o rewriter2 rewriter2.o ../ladr/libladr.a -lm $(LADRLIBS)

gen_trc_defs: gen_trc_defs.o
	$(CC) $(CFLAGS) -o gen_trc_defs gen_trc_defs.o ../ladr/libladr.a -lm $(LADRLIBS)





