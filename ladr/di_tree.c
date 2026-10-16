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

        git diff 25b4505 -- ladr/di_tree.c
*/

#include "di_tree.h"

/* Private definitions and types */

static int Nonunit_fsub_tests;
static int Nonunit_bsub_tests;

static unsigned Sub_calls = 0;
static unsigned Sub_calls_overflows = 0;
#define BUMP_SUB_CALLS {Sub_calls++; if (Sub_calls == 0) Sub_calls_overflows++;}

/*************
 *
 *   nonunit_fsub_tests(void)
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
int nonunit_fsub_tests(void)
{
  return Nonunit_fsub_tests;
}  /* nonunit_fsub_tests */

/*************
 *
 *   nonunit_bsub_tests(void)
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
int nonunit_bsub_tests(void)
{
  return Nonunit_bsub_tests;
}  /* nonunit_bsub_tests */

/*
 * memory management
 */

#define PTRS_DI_TREE PTRS(sizeof(struct di_tree))
static unsigned Di_tree_gets, Di_tree_frees;

/*************
 *
 *   Di_tree get_di_tree()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Di_tree get_di_tree(void)
{
  Di_tree p = get_cmem(PTRS_DI_TREE);
  Di_tree_gets++;
  return(p);
}  /* get_di_tree */

/*************
 *
 *    free_di_tree()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_di_tree(Di_tree p)
{
  free_mem(p, PTRS_DI_TREE);
  Di_tree_frees++;
}  /* free_di_tree */

/*************
 *
 *   fprint_di_tree_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to FILE *fp) memory usage statistics for data types
associated with the di_tree package.
The Boolean argument heading tells whether to print a heading on the table.
*/

/* PUBLIC */
void fprint_di_tree_mem(FILE *fp, BOOL heading)
{
  int n;
  if (heading)
    fprintf(fp, "  type (bytes each)        gets      frees     in use      bytes\n");

  n = sizeof(struct di_tree);
  fprintf(fp, "di_tree (%4d)      %11u%11u%11u%9.1f K\n",
          n, Di_tree_gets, Di_tree_frees,
          Di_tree_gets - Di_tree_frees,
          ((Di_tree_gets - Di_tree_frees) * n) / 1024.);

}  /* fprint_di_tree_mem */

/*************
 *
 *   p_di_tree_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to stdout) memory usage statistics for data types
associated with the di_tree package.
*/

/* PUBLIC */
void p_di_tree_mem(void)
{
  fprint_di_tree_mem(stdout, TRUE);
}  /* p_di_tree_mem */

/*
 *  end of memory management
 */

/*************
 *
 *   init_di_tree()
 *
 *************/

/* DOCUMENTATION
This routine allocates and returns an empty integer-vector
discrimination index.
*/

/* PUBLIC */
Di_tree init_di_tree(void)
{
  return get_di_tree();
}  /* init_di_tree */

/*************
 *
 *   di_tree_insert()
 *
 *************/

/* DOCUMENTATION
*/

/* Is (position, label) before (p, l) in sibling order? */

static
BOOL di_before(int position, int label, int p, int l)
{
  return position < p || (position == p && label < l);
}  /* di_before */

/* PUBLIC */
void di_tree_insert(I2list vec, Di_tree node, void *datum)
{
  /* Iterative: the dense version recursed once per feature position, so the
     recursion was as deep as the vector was long -- around 28,600 frames on a
     knowledge-base-sized input.  A sparse path is short, but there is no
     reason to recurse for it. */
  for (; vec; vec = vec->next) {
    Di_tree prev = NULL;
    Di_tree curr = node->kids;
    /* siblings are ordered by (position, label) */
    while (curr && di_before(curr->position, curr->label, vec->i, vec->j)) {
      prev = curr;
      curr = curr->next;
    }
    if (curr == NULL || curr->position != vec->i || curr->label != vec->j) {
      Di_tree new = get_di_tree();
      new->position = vec->i;
      new->label = vec->j;
      new->next = curr;
      new->kids = NULL;
      new->data = NULL;
      if (prev)
	prev->next = new;
      else
	node->kids = new;
      curr = new;
    }
    node = curr;
  }
  {
    Plist p = get_plist();
    p->v = datum;
    p->next = node->data;
    node->data = p;
  }
}  /* di_tree_insert */

/*************
 *
 *   di_tree_delete()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL di_tree_delete(I2list vec, Di_tree node, void *datum)
{
  if (vec == NULL) {
    node->data = plist_remove(node->data, datum);
    /* A node is worth keeping if anything still hangs off it.  In the dense
       tree only leaves held data, so "keep" meant "data remains"; a node can
       hold data AND children now, so both have to be checked. */
    return node->data != NULL || node->kids != NULL;
  }
  else {
    BOOL keep;
    Di_tree prev = NULL;
    Di_tree curr = node->kids;
    while (curr && di_before(curr->position, curr->label, vec->i, vec->j)) {
      prev = curr;
      curr = curr->next;
    }
    if (curr == NULL || curr->position != vec->i || curr->label != vec->j)
      fatal_error("di_tree_delete, node not found");
    keep = di_tree_delete(vec->next, curr, datum);
    if (keep)
      return TRUE;
    else {
      if (prev)
	prev->next = curr->next;
      else
	node->kids = curr->next;
      free_di_tree(curr);
      return node->data != NULL || node->kids != NULL;
    }
  }
}  /* di_tree_delete */

/*************
 *
 *   zap_di_tree()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void zap_di_tree(Di_tree node)
{
  /* No depth argument any more: paths have different lengths, so the tree is
     taken apart by its own structure rather than by a count the caller had to
     supply from feature_length(). */
  Di_tree kid;
  if (node == NULL)
    return;
  zap_plist(node->data);
  kid = node->kids;
  while (kid) {
    Di_tree tmp = kid;
    kid = kid->next;
    zap_di_tree(tmp);
  }
  free_di_tree(node);
}  /* zap_di_tree */

/*************
 *
 *   p_di_tree()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void p_di_tree(Di_tree node, int depth)
{
  int i;
  Di_tree kid;
  if (node == NULL)
    return;
  for (i = 0; i < depth; i++)
    printf(" ");
  if (depth == 0)
    printf("root");
  else
    printf("[%d]=%d", node->position, node->label);
  if (node->data) {
    Plist p;
    printf("  IDs:");
    for (p = node->data; p; p = p->next) {
      Topform c = p->v;
      printf(" %d", c->id);
    }
  }
  printf("\n");
  for (kid = node->kids; kid; kid = kid->next)
    p_di_tree(kid, depth+1);
}  /* p_di_tree */

/*************
 *
 *   subsume_di_literals()
 *
 *************/

static
BOOL subsume_di_literals(Literals clit, Context subst, Literals d, Trail *trp)
{
  BOOL subsumed = FALSE;
  Literals dlit;
  BUMP_SUB_CALLS;
  if (clit == NULL)
    return TRUE;
  else {
    for (dlit = d; !subsumed && dlit != NULL; dlit = dlit->next) {
      if (clit->sign == dlit->sign) {
	Trail mark = *trp;
	if (match(clit->atom, subst, dlit->atom, trp)) {
	  if (subsume_di_literals(clit->next, subst, d, trp))
	    subsumed = TRUE;
	  else {
	    undo_subst_2(*trp, mark);
	    *trp = mark;
	  }
	}
      }
    }
    return subsumed;
  }
}  /* subsume_di_literals */

/*************
 *
 *   subsumes_di()
 *
 *************/

static
BOOL subsumes_di(Literals c, Literals d, Context subst)
{
  Trail tr = NULL;
  BOOL subsumed = subsume_di_literals(c, subst, d, &tr);
  if (subsumed)
    undo_subst(tr);
  return subsumed;
}  /* subsumes_di */

/*************
 *
 *   di_tree_forward()
 *
 *************/

/* The two traversals.
 *
 * Both are written once, as a walk that calls back with the data at each node
 * it accepts, so that the filter the provers use and the filter
 * test.src/features_test.c checks against brute force are the same code.
 *
 * `vec` is the QUERY vector, sparse and in increasing position order.  An
 * absent position means zero, and every value stored in the tree is non-zero,
 * which is what makes the pruning below sound.
 */

/* The query's value at `position`, and the suffix of the query at or after
   it.  Sibling positions are non-decreasing, so the cursor only moves
   forward across one node's children. */

static
int di_value_at(I2list *vecp, int position)
{
  I2list v = *vecp;
  while (v && v->i < position)
    v = v->next;
  *vecp = v;
  return (v && v->i == position) ? v->j : 0;
}  /* di_value_at */

/* The suffix of `vec` strictly after `position`. */

static
I2list di_after(I2list vec, int position)
{
  while (vec && vec->i <= position)
    vec = vec->next;
  return vec;
}  /* di_after */

/* Walk every node whose path spells a stored vector <= vec.
 *
 * A node is reached only by descending children whose required value is met
 * by the query, so every constraint on the path already holds.  A stored
 * vector therefore qualifies exactly when its path has been walked -- which
 * is why data is offered at EVERY node, not only at the end of the vector.
 * The dense version could only accept at depth == length, because every path
 * had the same length.
 *
 * fn returns TRUE to stop the walk (forward subsumption wants the first hit).
 */

static
BOOL di_walk_le(I2list vec, Di_tree node,
		BOOL (*fn)(Plist, void *), void *state)
{
  Di_tree kid;
  BUMP_SUB_CALLS;

  if (node->data && fn(node->data, state))
    return TRUE;

  for (kid = node->kids; kid; kid = kid->next) {
    I2list cursor = vec;
    int qv = di_value_at(&cursor, kid->position);
    if (kid->label <= qv &&
	di_walk_le(di_after(cursor, kid->position), kid, fn, state))
      return TRUE;
    /* If kid->label > qv this child is pruned.  Later siblings at the same
       position have larger labels and are pruned too, but siblings at a
       larger position are not, so the loop continues rather than breaking. */
  }
  return FALSE;
}  /* di_walk_le */

/* Walk every node whose path spells a stored vector >= vec.
 *
 * Here the stored vector must cover every position the query constrains.
 * Walking the two in step: a child at a position BEFORE the next required one
 * is an entry the query does not constrain, so it is followed freely; a child
 * AT that position must meet the required value; and a child at a position
 * PAST it means this branch skips a position the query needs, so the stored
 * value there is zero and the branch cannot qualify.  Positions increase down
 * a path and across siblings, so that last case prunes every remaining
 * sibling as well.
 *
 * Data is accepted only once the query is exhausted -- anything earlier has
 * not covered the remaining constraints.
 */

static
void di_walk_ge(I2list vec, Di_tree node,
		void (*fn)(Plist, void *), void *state)
{
  Di_tree kid;
  BUMP_SUB_CALLS;

  if (vec == NULL && node->data)
    fn(node->data, state);

  for (kid = node->kids; kid; kid = kid->next) {
    if (vec == NULL || kid->position < vec->i)
      di_walk_ge(vec, kid, fn, state);
    else if (kid->position == vec->i) {
      if (kid->label >= vec->j)
	di_walk_ge(vec->next, kid, fn, state);
      /* labels ascend within a position, so a smaller one here just means
	 this child fails; later ones may still pass */
    }
    else
      break;                     /* every remaining sibling is past vec->i */
  }
}  /* di_walk_ge */

/* ---- the provers' two entry points, over those walks ---- */

struct fsub_state { Literals dlits; Context subst; Topform found; };

static
BOOL fsub_visit(Plist data, void *vstate)
{
  struct fsub_state *st = vstate;
  Plist p;
  for (p = data; p; p = p->next) {
    Topform c = p->v;
    Nonunit_fsub_tests++;
    if (subsumes_di(c->literals, st->dlits, st->subst)) {
      st->found = c;
      return TRUE;
    }
  }
  return FALSE;
}  /* fsub_visit */

struct bsub_state { Literals clits; Context subst; Plist subsumees; };

static
void bsub_visit(Plist data, void *vstate)
{
  struct bsub_state *st = vstate;
  Plist p;
  for (p = data; p; p = p->next) {
    Topform d = p->v;
    Nonunit_bsub_tests++;
    if (st->clits != d->literals && subsumes_di(st->clits, d->literals, st->subst))
      st->subsumees = plist_prepend(st->subsumees, d);
  }
}  /* bsub_visit */

/* ---- the filter alone, for the test suite ---- */

static
BOOL collect_le(Plist data, void *vstate)
{
  Plist *out = vstate;
  Plist p;
  for (p = data; p; p = p->next)
    *out = plist_prepend(*out, p->v);
  return FALSE;                  /* never stop; collect everything */
}  /* collect_le */

static
void collect_ge(Plist data, void *vstate)
{
  Plist *out = vstate;
  Plist p;
  for (p = data; p; p = p->next)
    *out = plist_prepend(*out, p->v);
}  /* collect_ge */

/* DOCUMENTATION
Every datum whose stored vector is <= vec componentwise.  The retrieval filter
without the subsumption test that normally follows it.
*/

/* PUBLIC */
Plist di_tree_data_le(I2list vec, Di_tree root)
{
  Plist out = NULL;
  di_walk_le(vec, root, collect_le, &out);
  return out;
}  /* di_tree_data_le */

/* DOCUMENTATION
Every datum whose stored vector is >= vec componentwise.
*/

/* PUBLIC */
Plist di_tree_data_ge(I2list vec, Di_tree root)
{
  Plist out = NULL;
  di_walk_ge(vec, root, collect_ge, &out);
  return out;
}  /* di_tree_data_ge */

/*************
 *
 *   forward_feature_subsume()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Topform forward_feature_subsume(Topform d, Di_tree root)
{
  I2list f = features(d->literals);
  struct fsub_state st;
  st.dlits = d->literals;
  st.subst = get_context();
  st.found = NULL;
  di_walk_le(f, root, fsub_visit, &st);
  free_context(st.subst);
  zap_i2list(f);
  return st.found;
}  /* forward_feature_subsume */

/*************
 *
 *   di_tree_back()
 *
 *************/

/* PUBLIC */
Plist back_feature_subsume(Topform c, Di_tree root)
{
  I2list f = features(c->literals);
  struct bsub_state st;
  st.clits = c->literals;
  st.subst = get_context();
  st.subsumees = NULL;
  di_walk_ge(f, root, bsub_visit, &st);
  free_context(st.subst);
  zap_i2list(f);
  return st.subsumees;
}  /* back_feature_subsume */

/*************
 *
 *   mega_sub_calls()
 *
 *************/

/* DOCUMENTATION
 */

/* PUBLIC */
unsigned mega_sub_calls(void)
{
  return
    (Sub_calls / 1000000) +
    ((UINT_MAX / 1000000) * Sub_calls_overflows);
}  /* mega_sub_calls */
