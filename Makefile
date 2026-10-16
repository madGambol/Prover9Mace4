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
# 2026-10-11 through 2026-10-16, and is not as William McCune released it in
# LADR-2009-11A. What changed and why is in FIXES.md; this file's own diff
# against the unmodified release is
#
#     git diff 25b4505 -- Makefile

help:
	@echo See README.make

all:
	/bin/mkdir -p bin
	cd ladr         && $(MAKE) lib
	cd mace4.src    && $(MAKE) all
	cd provers.src  && $(MAKE) all
	cd apps.src     && $(MAKE) all
	/bin/cp -p utilities/* bin
	@echo ""
	@echo "**** Now try 'make test1'. ****"
	@echo ""

ladr lib:
	cd ladr         && $(MAKE) lib

test1:
	bin/prover9 -f prover9.examples/x2.in | bin/prooftrans parents_only
	@echo ""
	@echo "**** If you see a proof, prover9 is probably okay. ****"
	@echo "**** Next try 'make test2'. ****"
	@echo ""

test2:
	bin/mace4 -v0 -f mace4.examples/group2.in | bin/interpformat tabular
	@echo ""
	@echo "**** If you see a group table, mace4 is probably okay. ****"
	@echo "**** Next try 'make test3'. ****"
	@echo ""

test3:
	bin/mace4 -n3 -m -1 < apps.examples/qg.in | bin/interpformat | bin/isofilter
	@echo ""
	@echo "*** If you see 5 interpretations, the apps are probably okay. ***"
	@echo ""
	@echo "*** All of the programs are in ./bin, and they can be copied anywhere you like. ***"
	@echo ""

clean:	
	cd ladr             && $(MAKE) realclean
	cd apps.src         && $(MAKE) realclean
	cd mace4.src        && $(MAKE) realclean
	cd provers.src      && $(MAKE) realclean

realclean:
	$(MAKE) clean
	/bin/rm -f bin/*


# The following cleans up, then makes a .tar.gz file of the current
# directory, leaving it in the parent directory.  (Gnu make only.)

DIR = $(shell basename $(PWD))

dist:
	$(MAKE) realclean
	cd .. && tar cvf $(DIR).tar $(DIR)
	gzip -f ../$(DIR).tar
	ls -l ../$(DIR).tar.gz
