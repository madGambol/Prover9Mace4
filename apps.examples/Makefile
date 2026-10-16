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

#  CHANGE NOTICE (GPL-2, section 2a): this file was modified by
#  Chris Arena on 2026-10-16, and is not as William McCune released it
#  in LADR-2009-11A.  The only change is the licence notice above,
#  which the release omitted from this file.

OUTPUTS = qg.out3 qg.iso3 MOL-cand.238  OL.8 OL.8.out OL.6 BA-sheffer.out \
	lattice-sax.rewritten meet-join-equations.out mjc01s-equations.out \
	OL.8.upper-covers

test:
	$(MAKE) backup
	./run-all

clean:
	/bin/rm -rf $(OUTPUTS) backup

backup:
	/bin/rm -rf backup
	mkdir backup && mv $(OUTPUTS) backup
