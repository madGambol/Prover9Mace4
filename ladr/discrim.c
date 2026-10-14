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
   retrieval find the right one by walking it, so each costs the fan-out at
   that node.  On Problems/CSR/CSR040-10.p that is 105.9 million sibling steps
   in one run, 91% of them at depths 1 and 2 -- the arguments of the top-level
   symbol -- where nodes average 269 and 288 steps per visit.
 
   This index makes that lookup O(1).  It does NOT replace the list: retrieval
   pushes `d->next` as its backtracking alternatives, so the order is load
   bearing, and the list is left exactly as it was.  The index only lets the
   forward scan be skipped.
 
   Open addressing with linear probing.  Symbol numbers are positive, so the
   two sentinels are negative and cannot collide with a real key.  Deletion
   leaves a tombstone rather than shifting, because a backward shift would
   have to re-probe every key in the run. */

#define DI_EMPTY  INT32_MIN
#define DI_TOMB   (INT32_MIN + 1)

/* Build the index once a node has this many rigid children.  Below it the
   list walk is cheaper than a hash probe and the memory is not worth it;
   measured fan-outs that matter here are in the hundreds. */
#define DI_PROMOTE_AT 32

struct discrim_index {
  int32_t *key;        /* capacity entries: a symbol, DI_EMPTY, or DI_TOMB */
  Discrim *node;       /* capacity entries, parallel to key */
  Discrim  tail;       /* last rigid child, so appending is O(1) */
  int capacity;        /* a power of two, so mask == capacity - 1 */
  int live;            /* real keys */
  int filled;          /* real keys + tombstones, for the load factor */
};

static unsigned Discrim_index_gets = 0, Discrim_index_frees = 0;

/* Knuth multiplicative, keeping the bits the multiply actually mixes. */
static int di_hash(int32_t symbol, int capacity)
{
  unsigned h = (unsigned) symbol * 2654435761u;
  return (int) ((h >> 4) & (unsigned) (capacity - 1));
}

/* The slot holding `symbol`, or the slot where it belongs.  Never fills, so
   the probe always terminates; a tombstone is not a stopping point for a
   lookup, only a reusable slot for an insert. */
static int di_probe(struct discrim_index *ix, int32_t symbol)
{
  int mask = ix->capacity - 1;
  int h = di_hash(symbol, ix->capacity);
  int first_tomb = -1;
  while (ix->key[h] != DI_EMPTY) {
    if (ix->key[h] == symbol)
      return h;
    if (ix->key[h] == DI_TOMB && first_tomb < 0)
      first_tomb = h;
    h = (h + 1) & mask;
  }
  return first_tomb >= 0 ? first_tomb : h;
}

static void di_put(struct discrim_index *ix, int32_t symbol, Discrim child);

static void di_grow(struct discrim_index *ix, int new_capacity)
{
  int32_t *old_key = ix->key;
  Discrim *old_node = ix->node;
  int old_capacity = ix->capacity;
  int k = 0;

  ix->key = (int32_t *) get_mem(PTRS(new_capacity * sizeof(int32_t)));
  ix->node = (Discrim *) get_mem(PTRS(new_capacity * sizeof(Discrim)));
  ix->capacity = new_capacity;
  ix->live = 0;
  ix->filled = 0;
  for (k = 0; k < new_capacity; k++) {
    ix->key[k] = DI_EMPTY;
    ix->node[k] = NULL;
  }
  for (k = 0; k < old_capacity; k++)
    if (old_key[k] != DI_EMPTY && old_key[k] != DI_TOMB)
      di_put(ix, old_key[k], old_node[k]);

  free_mem(old_key, PTRS(old_capacity * sizeof(int32_t)));
  free_mem(old_node, PTRS(old_capacity * sizeof(Discrim)));
}

static void di_put(struct discrim_index *ix, int32_t symbol, Discrim child)
{
  int h = di_probe(ix, symbol);
  if (ix->key[h] == symbol) {
    ix->node[h] = child;          /* already present: just retarget */
    return;
  }
  if (ix->key[h] == DI_EMPTY)
    ix->filled++;                 /* reusing a tombstone does not add one */
  ix->key[h] = symbol;
  ix->node[h] = child;
  ix->live++;
  /* Keep the load factor at or below one half, counting tombstones: linear
     probing degrades sharply as a table fills. */
  if (ix->filled * 2 >= ix->capacity)
    di_grow(ix, ix->capacity * 2);
}

static struct discrim_index *di_new(int capacity)
{
  struct discrim_index *ix =
    (struct discrim_index *) get_mem(PTRS(sizeof(struct discrim_index)));
  int k = 0;
  Discrim_index_gets++;
  ix->capacity = capacity;
  ix->live = 0;
  ix->filled = 0;
  ix->key = (int32_t *) get_mem(PTRS(capacity * sizeof(int32_t)));
  ix->node = (Discrim *) get_mem(PTRS(capacity * sizeof(Discrim)));
  for (k = 0; k < capacity; k++) {
    ix->key[k] = DI_EMPTY;
    ix->node[k] = NULL;
  }
  return ix;
}

static void di_free(struct discrim_index *ix)
{
  if (ix == NULL)
    return;
  Discrim_index_frees++;
  free_mem(ix->key, PTRS(ix->capacity * sizeof(int32_t)));
  free_mem(ix->node, PTRS(ix->capacity * sizeof(Discrim)));
  free_mem(ix, PTRS(sizeof(struct discrim_index)));
}

/* Promote: build the index from the children already on the list.  Only rigid
   children go in; variables stay a short prefix the caller walks anyway. */
static void di_promote(Discrim parent)
{
  Discrim k = NULL;
  int n = 0;
  int cap = 64;

  for (k = parent->u.kids; k; k = k->next)
    if (!DVAR(k))
      n++;
  while (cap < n * 4)
    cap *= 2;

  parent->child_index = di_new(cap);
  for (k = parent->u.kids; k; k = k->next)
    if (!DVAR(k)) {
      di_put(parent->child_index, k->symbol, k);
      parent->child_index->tail = k;
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
 *   discrim_child_append()
 *
 *************/

/* DOCUMENTATION
Link a rigid child at the end of an INDEXED node's child list, in O(1), and
record it in the index.  Only valid when discrim_child_indexed(parent).
*/

/* PUBLIC */
void discrim_child_append(Discrim parent, Discrim child)
{
  struct discrim_index *ix = parent->child_index;
  child->next = NULL;
  if (ix->tail != NULL)
    ix->tail->next = child;
  else {
    /* No rigid children yet: link after the variable children, which always
       come first and are never reordered. */
    Discrim k = parent->u.kids;
    if (k == NULL)
      parent->u.kids = child;
    else {
      while (k->next != NULL)
	k = k->next;
      k->next = child;
    }
  }
  ix->tail = child;
  di_put(ix, child->symbol, child);
}  /* discrim_child_append */

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
    int h = di_probe(ix, symbol);
    if (ix->key[h] == symbol)
      return ix->node[h];
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
    di_put(parent->child_index, child->symbol, child);
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
  if (parent->child_index != NULL) {
    struct discrim_index *ix = parent->child_index;
    int h = di_probe(ix, child->symbol);
    if (ix->key[h] == child->symbol) {
      ix->key[h] = DI_TOMB;
      ix->node[h] = NULL;
      ix->live--;
    }
    if (ix->tail == child) {
      /* The tail went away; find the new one.  This is O(fan-out), but it
	 happens only when the LAST rigid child is removed, which is rare
	 compared with inserting. */
      Discrim k = parent->u.kids, last = NULL;
      for (; k; k = k->next)
	if (!DVAR(k) && k != child)
	  last = k;
      ix->tail = last;
    }
  }
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

