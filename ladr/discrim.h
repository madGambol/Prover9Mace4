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

#ifndef TP_DISCRIM_H
#define TP_DISCRIM_H

#include <stdint.h>

#include "unify.h"
#include "index.h"

/* INTRODUCTION
This package implements two kinds of discrimination indexing
for first-order terms.  Both kinds support GENERALIZATION retrieval
only (e.g., for forward demodulation and forward subsumption).
<P>
The "wild" kind is an imperfect filter, and it does not bind variables.
The caller must also call a routine, say match(), to check if
the answers are really more general than the query term and to
construct the substitution.
Wild indexing supports associative-commutative (AC) and
commutative (C) symbols.
Indexing terms with AC symbols works by considering the number
of arguments and the number of nonvariable arguments of 
AC terms that do not occur in other AC terms.
(The term "wild" is used because all variables in the discrimination
tree are treated as the the wildcard symbol *).
<P>
With the "bind" kind, every answer is more general than the query term,
and the matching substitution is constructed during the retrieval.
Wild indexing supports commutative (C) symbols,
but it <I>does not support</I> associative-commutative (AC) symbols.
Retrieval with C symbols can produce duplicate answers.
<P>
There is probably a higher-level package (mindex ?) which
provides a uniform interface to these and other indexing methods.
*/
/* Public definitions */

typedef struct discrim * Discrim;

struct discrim {       /* node in a discrimination tree */
  Discrim   next;      /* sibling */
  union {
    Discrim kids;      /* for internal nodes */
    Plist data;        /* for leaves */
  } u;

  /* The node's key.  Which KIND of key depends on the `type` field below:

       type == DVARIABLE   a variable number, VARNUM(t)       <= MAX_VARS
       type == DRIGID      a SYMBOL number, SYMNUM(t)         unbounded
       AC indexing         an argument count, num_args        <= MAX_ARITY

     This was a `short`, and two of those three outgrow one.  Symbol numbers
     come from an incrementing counter with no cap -- SYM_TAB_SIZE is the width
     of the hash table, not a limit on symnums -- and MAX_ARITY is 65,534,
     which does not fit in a short at all.  Assigning an int symnum to a short
     truncates silently, and above 32,767 the value goes negative: symnum
     40,000 reads back as -25,536.

     That is not a slow degradation.  Insertion and retrieval both decide
     whether a node for this key exists by comparing `d->symbol != key`, and a
     truncated field never equals the untruncated local, so the term is never
     found again -- and the delete path then reaches
     fatal_error("discrim_bind_delete, cannot find end") and the prover EXITS.
     Measured in test.src/discrim_test.c, which has to fork to survive it.

     int32_t costs nothing: there were already five bytes of padding after the
     short, so struct discrim is 24 bytes either way. */

  /* An optional symbol-keyed index over this node's RIGID children, built
     lazily once there are enough of them to be worth it.  NULL on almost
     every node.

     Why: the children are a sorted linked list and both insert and retrieve
     find the right one by walking it, so the cost of each is the fan-out at
     that node.  Measured on Problems/CSR/CSR040-10.p, 105.9 million sibling
     steps in one run, and NOT where it looks: only 177,751 of them are at the
     root, while 96.8 million -- 91% -- are at depths 1 and 2, the ARGUMENTS of
     the top-level symbol.  Those nodes average 269 and 288 steps per visit.

     The list is deliberately left exactly as it was, because retrieval pushes
     `d->next` as its backtracking alternatives and so depends on the order.
     This index only lets the forward scan be SKIPPED; it never replaces the
     list.  See discrim_child_find() in discrim.c. */

  struct discrim_index *child_index;  /* NULL until promoted */

  int32_t symbol;      /* see above: a varnum, a symnum, or an arity */
  uint16_t nkids;      /* rigid children so far, saturating at the promotion
			  threshold; it fits in what was padding */
  char type;           /* term type and for ac indexing type */
};

typedef struct discrim_pos * Discrim_pos;

struct discrim_pos {  /* to save position in set of answers */
  void    *query;
  Context subst;        /* substitution */
  Plist   data;         /* identical terms from leaf of discrim tree */
  void    *backtrack;   /* data for backtracking */
};

/* type of discrimination tree node */

enum { DVARIABLE, DRIGID, AC_ARG_TYPE, AC_NV_ARG_TYPE };

#define DVAR(d)  ((d)->type == DVARIABLE)

/* The per-node child index.  Opaque; discrim.c owns it and free_discrim()
   releases it, so no caller has to think about its lifetime. */

struct discrim_index;

/* Find the rigid child of `parent` whose symbol is `symbol`, starting the
   search at `from` (which the caller has already advanced past the variable
   children).  Returns that child, or the first child at-or-after `from` whose
   symbol exceeds `symbol`, or NULL -- exactly what the hand-written forward
   scan returned, which is what makes it a drop-in replacement. */

Discrim discrim_child_find(Discrim parent, Discrim from, int32_t symbol);

/* Tell the index that a rigid child was added or removed.  Both are no-ops
   until the node has enough children to be promoted. */

void discrim_child_added(Discrim parent, Discrim child);

void discrim_child_removed(Discrim parent, Discrim child);

/* An indexed node's rigid children need not be sorted, so they can be
   appended in O(1) rather than spliced into position. */

BOOL discrim_child_indexed(Discrim parent);

void discrim_child_append(Discrim parent, Discrim child);

/* End of public definitions */

/* Public function prototypes from discrim.c */

Discrim get_discrim(void);

void free_discrim(Discrim p);

Discrim_pos get_discrim_pos(void);

void free_discrim_pos(Discrim_pos p);

void fprint_discrim_mem(FILE *fp, BOOL heading);

void p_discrim_mem(void);

Discrim discrim_init(void);

void discrim_dealloc(Discrim d);

void destroy_discrim_tree(Discrim d);

BOOL discrim_empty(Discrim d);

#endif  /* conditional compilation of whole file */
