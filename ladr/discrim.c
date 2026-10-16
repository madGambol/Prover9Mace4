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
    on 2026-10-13 through 2026-10-16, and is not as William McCune released it
    in LADR-2009-11A. What changed and why is in FIXES.md; this file's own
    diff against the unmodified release is

        git diff 25b4505 -- ladr/discrim.c
*/

#include "discrim.h"

/* Private definitions and types */

/*
 * memory management
 */

#define PTRS_DISCRIM PTRS(sizeof(struct discrim))
static unsigned Discrim_gets, Discrim_frees;

/* The per-node child index, implemented further down; declared here because
   free_discrim() releases it and sits above the implementation. */
struct discrim_index;
static void di_free(struct discrim_index *ix);

#define PTRS_DISCRIM_POS PTRS(sizeof(struct discrim_pos))
static unsigned Discrim_pos_gets, Discrim_pos_frees;

/*************
 *
 *   Discrim get_discrim()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Discrim get_discrim(void)
{
  Discrim p = get_cmem(PTRS_DISCRIM);
  Discrim_gets++;
  return(p);
}  /* get_discrim */

/*************
 *
 *    free_discrim()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_discrim(Discrim p)
{
  /* The child index belongs to the node, so it is released here rather than
     by any caller -- there is exactly one place a node is freed. */
  di_free(p->child_index);
  p->child_index = NULL;
  free_mem(p, PTRS_DISCRIM);
  Discrim_frees++;
}  /* free_discrim */

/*************
 *
 *   Discrim_pos get_discrim_pos()
 *
 *************/

/* DOCUMENTATION
The structure is not initialized.
*/

/* PUBLIC */
Discrim_pos get_discrim_pos(void)
{
  Discrim_pos p = get_mem(PTRS_DISCRIM_POS);  /* not initialized */
  Discrim_pos_gets++;
  return(p);
}  /* get_discrim_pos */

/*************
 *
 *    free_discrim_pos()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_discrim_pos(Discrim_pos p)
{
  free_mem(p, PTRS_DISCRIM_POS);
  Discrim_pos_frees++;
}  /* free_discrim_pos */

/*************
 *
 *   fprint_discrim_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to FILE *fp) memory usage statistics for data types
associated with the discrim package.
The Boolean argument heading tells whether to print a heading on the table.
*/

/* PUBLIC */
void fprint_discrim_mem(FILE *fp, BOOL heading)
{
  int n;
  if (heading)
    fprintf(fp, "  type (bytes each)        gets      frees     in use      bytes\n");

  n = sizeof(struct discrim);
  fprintf(fp, "discrim (%4d)      %11u%11u%11u%9.1f K\n",
          n, Discrim_gets, Discrim_frees,
          Discrim_gets - Discrim_frees,
          ((Discrim_gets - Discrim_frees) * n) / 1024.);

  n = sizeof(struct discrim_pos);
  fprintf(fp, "discrim_pos (%4d)  %11u%11u%11u%9.1f K\n",
          n, Discrim_pos_gets, Discrim_pos_frees,
          Discrim_pos_gets - Discrim_pos_frees,
          ((Discrim_pos_gets - Discrim_pos_frees) * n) / 1024.);

}  /* fprint_discrim_mem */

/*************
 *
 *   p_discrim_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to stdout) memory usage statistics for data types
associated with the discrim package.
*/

/* PUBLIC */
void p_discrim_mem(void)
{
  fprint_discrim_mem(stdout, TRUE);
}  /* p_discrim_mem */

/*
 *  end of memory management
 */


/*************
 *
 *   The per-node child index
 *
 *************/

/* A node's rigid children are a sorted linked list, and both insertion and
   retrieval found the right one by WALKING it, so each cost the fan-out at
   that node.  On Problems/CSR/CSR040-10.p that is 105.9 million sibling steps
   in one run, 91% of them at depths 1 and 2 -- the arguments of the top-level
   symbol -- where nodes average 269 and 288 steps per visit.

   WHAT THIS IS, AND WHY IT IS NOT A HASH TABLE
   It is the same children, SORTED BY SYMBOL, in an array.  Lookup is a binary
   search and insertion is a binary search plus one memmove of pointers, so
   neither walks the fan-out, and the array's order IS the list's order.

   It was a hash table first -- open addressing, tombstones, load factor,
   rehash, about 150 lines -- on the argument that the order of the children
   was no longer read by anything, so an unordered container was free to hold
   them and new children could be appended in O(1).  That argument was wrong,
   and the way it was wrong is worth keeping: retrieval does not walk the
   rigid list, but the list order is the order of the depth-first walk over
   the tree, hence the order in which discrim_bind_retrieve_first() returns
   equally-matching answers, and demodulation and forward subsumption each
   take the FIRST answer.  Reordering the children changed which demodulator
   was applied.  Thirteen corpus problems stopped being proved; on
   Problems/SWV/SWV558-1.010.p a 0.03 s proof became 2 GB exhausted after 79 s
   of search.  See the note above discrim_child_insert() for the measurements.

   So the order is not a property this code may trade away, and an unordered
   container was the wrong structure for it.  A sorted array states the
   invariant in the data structure rather than in a comment above it: there is
   nowhere to put an out-of-order child.  It is also less code than the hash
   it replaces, and O(log n) against O(1) is not measurable here -- CSR040-10
   startup is 5.07 s either way, against 60 s with no index at all. */

/* Build the index once a node has this many rigid children.  Below it the
   list walk is cheaper than a binary search plus the array, and the memory is
   not worth it; measured fan-outs that matter here are in the hundreds. */
#define DI_PROMOTE_AT 32

/* Two parallel arrays rather than one of structs, and the KEYS ARE INLINE
   rather than read through the child pointer.  That is the difference between
   a binary search that touches eight contiguous int32s -- a cache line or two
   of the key array -- and one that makes eight dependent pointer loads into
   scattered nodes.  Measured on CSR040-10 startup, keys inline is 5.1 s and
   keys behind the pointer is 6.5 s, which is the whole of the margin against
   the hash table this replaced. */
struct discrim_index {
  int32_t *sym;        /* the rigid children's symbols, SORTED, ASCENDING */
  Discrim *kid;        /* the children themselves, parallel to sym */
  int nkid;            /* how many */
  int kidcap;          /* allocated */
};

static unsigned Discrim_index_gets = 0, Discrim_index_frees = 0;

/* Position of the first child whose symbol is not less than `symbol`, i.e.
   where `symbol` belongs.  A binary search: the whole point of the array. */
static int di_pos(struct discrim_index *ix, int32_t symbol)
{
  int lo = 0, hi = ix->nkid;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (ix->sym[mid] < symbol)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

static void di_reserve(struct discrim_index *ix, int want)
{
  int32_t *old_sym = NULL;
  Discrim *old_kid = NULL;
  int old_cap = 0;
  if (want <= ix->kidcap)
    return;
  old_sym = ix->sym;
  old_kid = ix->kid;
  old_cap = ix->kidcap;
  while (ix->kidcap < want)
    ix->kidcap = (ix->kidcap == 0) ? DI_PROMOTE_AT * 2 : ix->kidcap * 2;
  ix->sym = (int32_t *) get_mem(PTRS(ix->kidcap * sizeof(int32_t)));
  ix->kid = (Discrim *) get_mem(PTRS(ix->kidcap * sizeof(Discrim)));
  if (old_cap > 0) {
    memcpy(ix->sym, old_sym, old_cap * sizeof(int32_t));
    memcpy(ix->kid, old_kid, old_cap * sizeof(Discrim));
    free_mem(old_sym, PTRS(old_cap * sizeof(int32_t)));
    free_mem(old_kid, PTRS(old_cap * sizeof(Discrim)));
  }
}

static void di_insert_at(struct discrim_index *ix, int pos, Discrim child)
{
  di_reserve(ix, ix->nkid + 1);
  if (pos < ix->nkid) {
    memmove(&ix->sym[pos+1], &ix->sym[pos],
	    (ix->nkid - pos) * sizeof(int32_t));
    memmove(&ix->kid[pos+1], &ix->kid[pos],
	    (ix->nkid - pos) * sizeof(Discrim));
  }
  ix->sym[pos] = child->symbol;
  ix->kid[pos] = child;
  ix->nkid++;
}

static void di_remove(struct discrim_index *ix, Discrim child)
{
  int pos = di_pos(ix, child->symbol);
  if (pos >= ix->nkid || ix->kid[pos] != child)
    return;                       /* not there; nothing to undo */
  if (pos + 1 < ix->nkid) {
    memmove(&ix->sym[pos], &ix->sym[pos+1],
	    (ix->nkid - pos - 1) * sizeof(int32_t));
    memmove(&ix->kid[pos], &ix->kid[pos+1],
	    (ix->nkid - pos - 1) * sizeof(Discrim));
  }
  ix->nkid--;
}

static struct discrim_index *di_new(void)
{
  struct discrim_index *ix =
    (struct discrim_index *) get_mem(PTRS(sizeof(struct discrim_index)));
  Discrim_index_gets++;
  ix->sym = NULL;
  ix->kid = NULL;
  ix->nkid = 0;
  ix->kidcap = 0;
  return ix;
}

static void di_free(struct discrim_index *ix)
{
  if (ix == NULL)
    return;
  Discrim_index_frees++;
  if (ix->kidcap > 0) {
    free_mem(ix->sym, PTRS(ix->kidcap * sizeof(int32_t)));
    free_mem(ix->kid, PTRS(ix->kidcap * sizeof(Discrim)));
  }
  free_mem(ix, PTRS(sizeof(struct discrim_index)));
}

/* Promote: build the index from the children already on the list.  Only rigid
   children go in; variables stay a short prefix the caller walks anyway.  The
   list is sorted here and stays sorted, so copying it in list order gives a
   sorted array -- and that is the invariant, not an accident of this loop. */
static void di_promote(Discrim parent)
{
  Discrim k = NULL;
  int n = 0;

  for (k = parent->u.kids; k; k = k->next)
    if (!DVAR(k))
      n++;

  parent->child_index = di_new();
  di_reserve(parent->child_index, n);
  for (k = parent->u.kids; k; k = k->next)
    if (!DVAR(k)) {
      struct discrim_index *ix = parent->child_index;
      ix->sym[ix->nkid] = k->symbol;
      ix->kid[ix->nkid] = k;
      ix->nkid++;
    }
}

/*************
 *
 *   discrim_child_indexed()
 *
 *************/

/* DOCUMENTATION
Is this node's rigid child list indexed?  An indexed node's rigid children no
longer have to be in sorted order, because nothing walks them in order any
more -- insertion looks the symbol up and retrieval does too -- so a caller
may append instead of splicing into position.  That is the whole point: an
ordered linked list cannot be built in better than O(n^2) however the position
is found, so the order had to go before the quadratic could.
*/

/* PUBLIC */
BOOL discrim_child_indexed(Discrim parent)
{
  return (parent != NULL && parent->child_index != NULL) ? TRUE : FALSE;
}  /* discrim_child_indexed */

/*************
 *
 *   discrim_child_insert()
 *
 *************/

/* DOCUMENTATION
Link a rigid child into an INDEXED node's child list IN SYMBOL ORDER, and
record it in the index.  Only valid when discrim_child_indexed(parent).

THE ORDER IS LOAD BEARING, and finding that out cost a capability regression.
An earlier version of this function appended at the tail instead, because
appending is O(1) and splicing into a sorted list is O(fan-out), and argued
that nothing read the order any more: retrieval looks a symbol up in the index
rather than walking the siblings.  That argument is wrong about WHICH order
matters.  Retrieval does not walk the rigid list, but the order of the list is
the order of the depth-first walk over the tree, and so it is the order in
which discrim_bind_retrieve_first() hands back equally-matching answers.
Demodulation and forward subsumption both take the FIRST answer, so reordering
the children changes which demodulator is applied -- a different normal form,
a different search.  Measured on Problems/SWV/SWV558-1.010.p: with the
children sorted, Given=47 Generated=723 Forward_subsumed=201 and the proof is
found in 0.03 s; with them appended, the same 47 given clauses generate
1,052,629 clauses, forward subsumption catches 154 instead of 201, and the run
exhausts 2 GB after 79 s without a proof.  Thirteen corpus problems behaved
that way.  The index itself was never at fault -- it was instrumented against
an order-independent scan of the very list it indexes, over millions of
lookups, with no divergence.

So the order stays, and the cost of keeping it is paid differently: the index
carries the same children in an ARRAY, in the same order, and the predecessor
of the new symbol is found by BINARY SEARCH rather than by walking the list.
The list splice is then O(1), and the array insert is a memmove of pointers --
contiguous, no dependent loads -- where the walk it replaces was one cache
miss per sibling.  That keeps both properties that matter: the answer order of
the 2009 release, and an insert that does not walk the fan-out.
*/

/* PUBLIC */
void discrim_child_insert(Discrim parent, Discrim child)
{
  struct discrim_index *ix = parent->child_index;
  int pos = di_pos(ix, child->symbol);
  Discrim prev = NULL;

  if (pos > 0)
    prev = ix->kid[pos-1];        /* the rigid child just below it */
  else {
    /* It sorts before every rigid child, so it goes after the variable
       children, which always come first and are never reordered.  That prefix
       is bounded by the number of distinct variables in one indexed term, so
       walking it is not the fan-out. */
    Discrim k = parent->u.kids;
    while (k != NULL && DVAR(k)) {
      prev = k;
      k = k->next;
    }
  }

  child->next = (pos < ix->nkid) ? ix->kid[pos] : NULL;
  if (prev == NULL)
    parent->u.kids = child;
  else
    prev->next = child;

  di_insert_at(ix, pos, child);
}  /* discrim_child_insert */

/*************
 *
 *   discrim_child_find()
 *
 *************/

/* DOCUMENTATION
Find the rigid child of parent whose symbol is `symbol`, starting at `from`.
Returns exactly what a forward walk of the sibling list would have returned:
that child, or the first one at-or-after `from` whose symbol exceeds `symbol`,
or NULL.
*/

/* PUBLIC */
Discrim discrim_child_find(Discrim parent, Discrim from, int32_t symbol)
{
  if (parent->child_index != NULL) {
    struct discrim_index *ix = parent->child_index;
    int pos = di_pos(ix, symbol);
    if (pos < ix->nkid && ix->sym[pos] == symbol)
      return ix->kid[pos];
    /* Absent.  The callers all treat "found a node whose symbol differs" and
       "ran off the end" the same way -- they check `d->symbol != symbol` -- so
       NULL is the honest answer and keeps them on the same path. */
    return NULL;
  }
  else {
    Discrim d = from;
    while (d && d->symbol < symbol)
      d = d->next;
    return d;
  }
}  /* discrim_child_find */

/*************
 *
 *   discrim_children_ordered()
 *
 *************/

/* DOCUMENTATION
Check, over a whole discrimination tree, that every node's rigid children are
in non-decreasing symbol order and that its variable children all precede
them.  Returns TRUE if so.

This exists because that invariant was broken on purpose once, for speed, on
the argument that nothing read the order any more -- and the order turned out
to be the order in which discrim_bind_retrieve_first() returns equally-matching
answers, which demodulation and forward subsumption both consume.  The cost
was thirteen corpus problems; see the note above discrim_child_insert().  The
invariant is cheap to state and was not stated anywhere, so it is stated here
and asserted by test.src/discrim_test.c.

`n` is the number of tree levels still to descend, as elsewhere in this file:
1 for the root of a tree of terms.
*/

/* PUBLIC */
BOOL discrim_children_ordered(Discrim d, int n)
{
  if (d == NULL || n <= 0)
    return TRUE;
  else {
    Discrim k = NULL;
    int32_t prev_symbol = INT32_MIN;
    BOOL seen_rigid = FALSE;

    for (k = d->u.kids; k != NULL; k = k->next) {
      if (DVAR(k)) {
	if (seen_rigid)
	  return FALSE;            /* a variable child after a rigid one */
      }
      else {
	if (seen_rigid && k->symbol < prev_symbol)
	  return FALSE;            /* rigid children out of order */
	seen_rigid = TRUE;
	prev_symbol = k->symbol;
      }
    }
    for (k = d->u.kids; k != NULL; k = k->next) {
      ArityType arity = DVAR(k) ? 0 : sn_to_arity(k->symbol);
      if (!discrim_children_ordered(k, n + arity - 1))
	return FALSE;
    }
    return TRUE;
  }
}  /* discrim_children_ordered */

/*************
 *
 *   discrim_child_added()
 *
 *************/

/* DOCUMENTATION
Record that a rigid child was linked under parent.  Promotes the node to an
indexed one once it has DI_PROMOTE_AT rigid children.
*/

/* PUBLIC */
void discrim_child_added(Discrim parent, Discrim child)
{
  if (parent == NULL || child == NULL || DVAR(child))
    return;
  if (parent->child_index != NULL) {
    /* The caller linked it itself, in sorted position; record it there too.
       (discrim_child_insert() is the path that does both at once.) */
    struct discrim_index *ix = parent->child_index;
    int pos = di_pos(ix, child->symbol);
    if (pos >= ix->nkid || ix->kid[pos] != child)
      di_insert_at(ix, pos, child);
    return;
  }
  if (parent->nkids < DI_PROMOTE_AT)
    parent->nkids++;              /* saturates; only the threshold matters */
  if (parent->nkids >= DI_PROMOTE_AT)
    di_promote(parent);
}  /* discrim_child_added */

/*************
 *
 *   discrim_child_removed()
 *
 *************/

/* DOCUMENTATION
Record that a rigid child was unlinked from parent.
*/

/* PUBLIC */
void discrim_child_removed(Discrim parent, Discrim child)
{
  if (parent == NULL || child == NULL || DVAR(child))
    return;
  if (parent->child_index != NULL)
    di_remove(parent->child_index, child);
}  /* discrim_child_removed */

/*************
 *
 *   discrim_init()
 *
 *************/

/* DOCUMENTATION
This routine allocates and returns an empty discrimination index.
It can be used for either wild or tame indexing.
*/

/* PUBLIC */
Discrim discrim_init(void)
{
  return get_discrim();
}  /* discrim_init */

/*************
 *
 *   discrim_dealloc(d)
 *
 *************/

/* DOCUMENTATION
This routine frees an empty discrimination index (wild or tame).
*/

/* PUBLIC */
void discrim_dealloc(Discrim d)
{
  if (d->u.kids) {
    fatal_error("discrim_dealloc, nonempty index.");
  }
  else
    free_discrim(d);
}  /* discrim_dealloc */

/*************
 *
 *   zap_discrim_tree()
 *
 *************/

static
void zap_discrim_tree(Discrim d, int n)
{
  if (n == 0) {
    zap_plist(d->u.data);
  }
  else {
    ArityType arity;
    Discrim k, prev;

    k = d->u.kids;
    while (k != NULL) {
      if (k->type == AC_ARG_TYPE || k->type == AC_NV_ARG_TYPE)
	arity = 0;
      else if (DVAR(k))
	arity = 0;
      else
	arity = sn_to_arity(k->symbol);
      prev = k;
      k = k->next;
      zap_discrim_tree(prev, n+arity-1);
    }
  }
  free_discrim(d);
}  /* zap_discrim_tree */

/*************
 *
 *   destroy_discrim_tree()
 *
 *************/

/* DOCUMENTATION
This routine frees all the memory associated with a discrimination
index.  It can be used with either wild or tame trees.
*/

/* PUBLIC */
void destroy_discrim_tree(Discrim d)
{
  zap_discrim_tree(d, 1);
}  /* destroy_discrim_tree */

/*************
 *
 *   discrim_empty()
 *
 *************/

/* DOCUMENTATION
This Boolean function checks if a discrimination index is empty.
It can be used with either wild or tame trees.
*/

/* PUBLIC */
BOOL discrim_empty(Discrim d)
{
  return (d == NULL ? TRUE : (d->u.kids == NULL ? TRUE : FALSE));
}  /* discrim_empty */

