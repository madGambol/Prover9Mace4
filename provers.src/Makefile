# CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena on
# 2026-10-10 through 2026-10-14, and is not as William McCune released it in
# LADR-2009-11A. What changed and why is in FIXES.md; this file's own diff
# against the unmodified release is
#
#     git diff 25b4505 -- provers.src/Makefile

# LADR/prover9.src

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
# CFLAGS = $(XFLAGS) -Wall -pedantic

PRVR_OBJ = index_lits.o forward_subsume.o demodulate.o\
           pred_elim.o unfold.o semantics.o\
	   giv_select.o white_black.o actions.o\
	   search.o\
	   utilities.o\
	   provers.o\
	   foffer.o

OBJECTS = $(PRVR_OBJ)

# PROGRAMS = mprover iterate4

PROGRAMS = prover9 fof-prover9 autosketches4 newauto newsax\
	   ladr_to_tptp tptp_to_ladr

##############################################################################

all: libs $(PROGRAMS) install clean

libs: ladr libmace4

ladr libladr:
	cd ../ladr && $(MAKE) libladr
	$(MAKE) clean

libmace libmace4:
	cd ../mace4.src && $(MAKE) libmace4
	$(MAKE) clean

install:
	/bin/mkdir -p ../bin
	/bin/cp -p $(PROGRAMS) ../bin

clean:
	/bin/rm -f *.o

realclean:
	/bin/rm -f *.o $(PROGRAMS)

protos:
	util/make_protos $(OBJECTS)

dep:
	util/make_dep $(OBJECTS)

tags:
	etags *.c ../ladr/*.c

prover:
	/bin/rm -f prover9
	$(MAKE) prover9

prover9: prover9.o $(OBJECTS)
	$(CC) $(CFLAGS) -o prover9 prover9.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

fof-prover9: fof-prover9.o $(OBJECTS)
	$(CC) $(CFLAGS) -o fof-prover9 fof-prover9.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

ladr_to_tptp: ladr_to_tptp.o $(OBJECTS)
	$(CC) $(CFLAGS) -o ladr_to_tptp ladr_to_tptp.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

tptp_to_ladr: tptp_to_ladr.o $(OBJECTS)
	$(CC) $(CFLAGS) -o tptp_to_ladr tptp_to_ladr.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

autosketches4: autosketches4.o $(OBJECTS)
	$(CC) $(CFLAGS) -o autosketches4 autosketches4.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

newauto: newauto.o $(OBJECTS)
	$(CC) $(CFLAGS) -o newauto newauto.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

newsax: newsax.o $(OBJECTS)
	$(CC) $(CFLAGS) -o newsax newsax.o $(OBJECTS) ../ladr/libladr.a -lm $(LADRLIBS)

cgrep: cgrep.o $(OBJECTS)
	$(CC) $(CFLAGS) -o cgrep cgrep.o $(OBJECTS) ../ladr/libladr.a

mprover: mprover.o $(OBJECTS)
	$(CC) $(CFLAGS) -o mprover mprover.o $(OBJECTS) ../ladr/libladr.a ../mace4.src/libmace4.a

iterate4: iterate4.o $(OBJECTS)
	$(CC) $(CFLAGS) -o iterate4 iterate4.o $(OBJECTS) ../ladr/libladr.a

prover9.o mprover.o iterate4.o autosketches4.o fof-prover9.o: search.h utilities.h  forward_subsume.h giv_select.h white_black.h demodulate.h actions.h index_lits.h pred_elim.h unfold.h provers.h

# The rest of the file is generated automatically by util/make_dep

index_lits.o:   	index_lits.h ../ladr/clock.h ../ladr/subsume.h ../ladr/di_tree.h

forward_subsume.o:   	forward_subsume.h ../ladr/subsume.h ../ladr/clock.h

demodulate.o:   	demodulate.h ../ladr/ladr.h

pred_elim.o:   	pred_elim.h ../ladr/subsume.h ../ladr/resolve.h ../ladr/clauses.h ../ladr/clause_misc.h ../ladr/ioutil.h

unfold.o:   	unfold.h ../ladr/parautil.h ../ladr/clist.h ../ladr/ioutil.h

semantics.o:   	semantics.h ../ladr/interp.h ../ladr/ioutil.h

giv_select.o:   	giv_select.h search-structures.h

white_black.o:   	white_black.h search-structures.h

actions.o:   	actions.h search-structures.h

search.o:   	search.h search-structures.h semantics.h pred_elim.h demodulate.h index_lits.h forward_subsume.h unfold.h actions.h giv_select.h white_black.h utilities.h ../ladr/interp.h ../ladr/ioutil.h ../ladr/subsume.h ../ladr/resolve.h ../ladr/clauses.h ../ladr/clause_misc.h ../ladr/ioutil.h ../ladr/ladr.h ../ladr/clock.h ../ladr/subsume.h ../ladr/di_tree.h ../ladr/subsume.h ../ladr/clock.h ../ladr/parautil.h ../ladr/clist.h ../ladr/ioutil.h

utilities.o:   	utilities.h search-structures.h

provers.o:   	provers.h search.h search-structures.h semantics.h pred_elim.h demodulate.h index_lits.h forward_subsume.h unfold.h actions.h giv_select.h white_black.h utilities.h ../ladr/interp.h ../ladr/ioutil.h ../ladr/subsume.h ../ladr/resolve.h ../ladr/clauses.h ../ladr/clause_misc.h ../ladr/ioutil.h ../ladr/ladr.h ../ladr/clock.h ../ladr/subsume.h ../ladr/di_tree.h ../ladr/subsume.h ../ladr/clock.h ../ladr/parautil.h ../ladr/clist.h ../ladr/ioutil.h

foffer.o:   	foffer.h search.h search-structures.h semantics.h pred_elim.h demodulate.h index_lits.h forward_subsume.h unfold.h actions.h giv_select.h white_black.h utilities.h ../ladr/interp.h ../ladr/ioutil.h ../ladr/subsume.h ../ladr/resolve.h ../ladr/clauses.h ../ladr/clause_misc.h ../ladr/ioutil.h ../ladr/ladr.h ../ladr/clock.h ../ladr/subsume.h ../ladr/di_tree.h ../ladr/subsume.h ../ladr/clock.h ../ladr/parautil.h ../ladr/clist.h ../ladr/ioutil.h
