/*  Copyright (C) 2006, 2007 William McCune

    This file is part of the LADR Deduction Library.

    The LADR Deduction Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License,
    version 2.

    The LADR Deduction Library is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the LADR Deduction Library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*  CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena
    on 2026-10-13, and is not as William McCune released it in LADR-2009-11A.
    What changed and why is in FIXES.md; this file's own diff against the
    unmodified release is

        git diff 25b4505 -- ladr/flatterm.h
*/

#ifndef TP_FLATTERM_H
#define TP_FLATTERM_H

#include "term.h"

/* INTRODUCTION
<P>
The Term macros VARIABLE(f), CONSTANT(f), COMPLEX(f), SYMNUM(f),
VARNUM(f), ARITY(f) are used for Flatterms as well.  The Term macro
ARG(t,i) is NOT used for Flatterms.
<P>
Traversing Flatterms.  It can be done recursively or iteratively.
When building flatterms, recursive is better, because you have to
make a Flatterm point to its end.

Iterative:

   Flatterm fi;
   for (f = fi; fi != f->end->next; fi = fi->next)
      ...

Recursive:

   int i;
   Flatterm fi = f->next;
   for (i = 0; i < ARITY(f); i++) {
     ...
     fi = fi->end->next;
   }
   
*/

/* Public definitions */

typedef struct flatterm * Flatterm;

struct flatterm {
  short         private_symbol; /* const/func/pred/var symbol ID */
  ArityType         arity;          /* number of arguments */
  Flatterm prev, next, end;

  /* The rest of the fields are for index retrieval and demodulation. */
  
  int size;                      /* symbol count */
  struct discrim *alternative;   /* subtree to try next */
  int varnum_bound_to;           /* -1 for not bound */
  BOOL reduced_flag;             /* fully demodulated */
};

/* Flatterm has its own accessors for the same reason struct term does: its
   private_symbol field holds either a variable number or a rigid symbol
   number, distinguished by sign.

   These exist separately because a Flatterm is not a Term.  The term macros
   used to be applied to flatterms as well -- they are untyped macros and both
   structs happen to have a field of that name, so it compiled -- and naming
   the quantities is what brought that to light.  Fifteen uses in flatterm.c
   were reaching through the wrong type's accessor. */

#define FLAT_VARIABLE(f) ((f)->private_symbol >= 0)
#define FLAT_SYMNUM(f)   ((SymNumType) (-((f)->private_symbol)))
#define FLAT_VARNUM(f)   ((VarNumType) ((f)->private_symbol))

/* End of public definitions */

/* Public function prototypes from flatterm.c */

Flatterm get_flatterm(void);

void free_flatterm(Flatterm p);

void fprint_flatterm_mem(FILE *fp, BOOL heading);

void p_flatterm_mem();

BOOL flatterm_ident(Flatterm a, Flatterm b);

void zap_flatterm(Flatterm f);

Flatterm term_to_flatterm(Term t);

Term flatterm_to_term(Flatterm f);

Flatterm copy_flatterm(Flatterm f);

void print_flatterm(Flatterm f);

int flatterm_symbol_count(Flatterm f);

void p_flatterm(Flatterm f);

BOOL flat_occurs_in(Flatterm t1, Flatterm t2);

I2list flat_multiset_vars(Flatterm f);

BOOL flat_variables_multisubset(Flatterm a, Flatterm b);

int flatterm_count_without_vars(Flatterm f);

#endif  /* conditional compilation of whole file */
