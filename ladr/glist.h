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

#ifndef TP_GLIST_H
#define TP_GLIST_H

#include "order.h"
#include "string.h"

/* INTRODUCTION
This package handles
Plist (singly-linked list of void pointers),
Ilist (singly-linked list of integers),
I2list (singly-linked list of <integer,integer> pairs),
I3list (singly-linked list of <integer,integer,integer> triples).
*/

/* Public definitions */

typedef struct plist * Plist;

struct plist {
  void *v;
  Plist next;
};

typedef struct ilist * Ilist;

struct ilist {
  int i;
  Ilist next;
};

typedef struct i2list * I2list;

struct i2list {
  int i;
  int j;
  I2list next;
};

typedef struct i3list * I3list;

struct i3list {
  int i;
  int j;
  int k;
  I3list next;
};

/* End of public definitions */

/* Public function prototypes from glist.c */

Ilist get_ilist(void);

void free_ilist(Ilist p);

Plist get_plist(void);

void free_plist(Plist p);

I2list get_i2list(void);

void free_i2list(I2list p);

I3list get_i3list(void);

void free_i3list(I3list p);

void fprint_glist_mem(FILE *fp, BOOL heading);

void p_glist_mem();

Plist plist_cat(Plist p1, Plist p2);

Plist plist_cat2(Plist p1, Plist p2);

Plist plist_pop(Plist p);

int plist_count(Plist p);

Plist reverse_plist(Plist p);

Plist copy_plist(Plist p);

void zap_plist(Plist p);

Plist plist_append(Plist lst, void *v);

Plist plist_prepend(Plist lst, void *v);

BOOL plist_member(Plist lst, void *v);

Plist plist_subtract(Plist p1, Plist p2);

BOOL plist_subset(Plist a, Plist b);

Plist plist_remove(Plist p, void *v);

Plist plist_remove_string(Plist p, char *s);

Plist sort_plist(Plist objects,	Ordertype (*comp_proc) (void *, void *));

Plist plist_remove_last(Plist p);

int position_of_string_in_plist(char *s, Plist p);

BOOL string_member_plist(char *s, Plist p);

int longest_string_in_plist(Plist p);

void *ith_in_plist(Plist p, int i);

void *plist_last(Plist p);

Ilist ilist_cat(Ilist p1, Ilist p2);

Ilist ilist_cat2(Ilist p1, Ilist p2);

Ilist ilist_pop(Ilist p);

int ilist_count(Ilist p);

Ilist reverse_ilist(Ilist p);

Ilist copy_ilist(Ilist p);

void zap_ilist(Ilist p);

Ilist ilist_append(Ilist lst, int i);

Ilist ilist_prepend(Ilist lst, int i);

Ilist ilist_last(Ilist lst);

BOOL ilist_member(Ilist lst, int i);

Ilist ilist_subtract(Ilist p1, Ilist p2);

Ilist ilist_removeall(Ilist p, int i);

Ilist ilist_intersect(Ilist a, Ilist b);

Ilist ilist_union(Ilist a, Ilist b);

Ilist ilist_set(Ilist m);

Ilist ilist_rem_dups(Ilist m);

BOOL ilist_is_set(Ilist a);

BOOL ilist_subset(Ilist a, Ilist b);

void fprint_ilist(FILE *fp, Ilist p);

void p_ilist(Ilist p);

Ilist ilist_copy(Ilist p);

Ilist ilist_remove_last(Ilist p);

int ilist_occurrences(Ilist p, int i);

Ilist ilist_insert_up(Ilist p, int i);

int position_in_ilist(int i, Ilist p);

void zap_i2list(I2list p);

I2list i2list_append(I2list lst, int i, int j);

I2list i2list_prepend(I2list lst, int i, int j);

I2list i2list_removeall(I2list p, int i);

I2list i2list_member(I2list lst, int i);

void p_i2list(I2list p);

int i2list_count(I2list p);

BOOL i3list_member(I3list lst, int i, int j, int k);

I3list i3list_append(I3list lst, int i, int j, int k);

I3list i3list_prepend(I3list lst, int i, int j, int k);

void zap_i3list(I3list p);

I3list reverse_i3list(I3list p);

I3list copy_i3list(I3list p);

int i3list_count(I3list p);

I2list alist_insert(I2list p, int key, int val);

int assoc(I2list p, int key);

I3list alist2_insert(I3list p, int key, int a, int b);

int assoc2a(I3list p, int key);

int assoc2b(I3list p, int key);

I3list alist2_remove(I3list p, int key);

BOOL i2list_multimember(I2list b, int i, int n);

BOOL i2list_multisubset(I2list a, I2list b);

I2list multiset_add_n(I2list a, int i, int n);

I2list multiset_add(I2list a, int i);

I2list multiset_union(I2list a, I2list b);

Ilist multiset_to_set(I2list m);

int multiset_occurrences(I2list m, int i);

/* Appending in O(1), with the caller holding the tail.  A Plist is just a
   pointer to its first cell, so plist_append() has nowhere to keep a tail and
   must walk; "X = plist_append(X, v)" in a loop is therefore quadratic.  See
   the long comment above plist_tail_append() in glist.c. */

Plist plist_tail_append(Plist lst, Plist *tail, void *v);

Ilist ilist_tail_append(Ilist lst, Ilist *tail, int i);

I2list i2list_tail_append(I2list lst, I2list *tail, int i, int j);

I3list i3list_tail_append(I3list lst, I3list *tail, int i, int j, int k);

/* An accumulator for building a large multiset.
 *
 * multiset_add_n() searches the I2list for the element it is adding, so
 * building a multiset of N distinct elements costs O(N^2) comparisons.  That
 * is fine for the multiset of variables in a clause, which MAX_VARS caps at
 * 100, and it is not fine for the multiset of symbols in an input file, which
 * has no cap at all.  See the comment on multiset_add_n() in glist.c for the
 * measurements.
 *
 * I2accum is the same abstraction -- a multiset of ints -- with O(1) amortised
 * insertion, backed by a hash table.  Build with i2accum_add_n(), then call
 * i2accum_to_i2list() once to get an ordinary I2list for everything that
 * consumes one.  The list comes back in first-seen order, which is the order
 * multiset_add_n() produces, so the two are interchangeable.
 */

typedef struct i2accum * I2accum;

I2accum get_i2accum(void);

void zap_i2accum(I2accum acc);

void i2accum_add_n(I2accum acc, int i, int n);

void i2accum_add(I2accum acc, int i);

int i2accum_occurrences(I2accum acc, int i);

int i2accum_distinct(I2accum acc);

I2list i2accum_to_i2list(I2accum acc);

Ilist i2accum_to_set(I2accum acc);

I2accum i2list_to_i2accum(I2list m);

#endif  /* conditional compilation of whole file */
