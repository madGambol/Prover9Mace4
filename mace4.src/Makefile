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
#     git diff 25b4505 -- mace4.src/Makefile

# LADR/mace4.src

CC = gcc

# XFLAGS can be specified on the command line (see XFLAGS below)

CFLAGS = $(XFLAGS) -O -Wall

# libladr.a contains one C++ translation unit (i2accum_cxx.cpp), so the C++
# standard library has to be linked wherever libladr is linked.  -lstdc++
# rather than linking with g++, so that CC and CFLAGS stay as they were.
LADRLIBS = -lstdc++
# CFLAGS = $(XFLAGS) -O6 -Wall
# CFLAGS = $(XFLAGS) -g  -O -Wall
# CFLAGS = $(XFLAGS) -g  -Wall
# CFLAGS = $(XFLAGS) -pg -O -Wall
# CFLAGS = $(XFLAGS)  -Wall -pedantic

OBJECTS = estack.o util.o print.o syms.o ground.o arithmetic.o select.o \
	propagate.o mstate.o negpropindex.o negprop.o ordercells.o \
	commandline.o msearch.o

all: ladr lib mace4 install

libmace4.a: $(OBJECTS)
	$(AR) rs libmace4.a $(OBJECTS)

##############################################################################

lib libmace4:
	$(MAKE) libmace4.a

ladr:
	cd ../ladr && $(MAKE) libladr.a
	$(MAKE) clean

mace4: libmace4.a mace4.o $(OBJECTS)
	$(CC) $(CFLAGS) -o mace4 mace4.o libmace4.a ../ladr/libladr.a -lm $(LADRLIBS)

$(OBJECTS): estack.h syms.h ground.h propagate.h mstate.h msearch.h

tags:
	etags *.c ../ladr/*.c

clean:
	/bin/rm -f *.o

realclean:
	/bin/rm -f *.o *.a mace4

install:
	/bin/mkdir -p ../bin
	/bin/mv mace4 ../bin
