/*  Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.

    This file is part of the LADR Deduction Library.

    NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
    not write this file.  It was written for the audit of his release that
    FIXES.md describes, and carries the library's licence so that it can
    be distributed with it.

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

/* Regression test for the sparse feature-vector index in ladr/di_tree.c.
 *
 * WHAT CHANGED
 *   Subsumption is expensive, so Prover9 filters candidates with a vector of
 *   small non-negative integers per clause, chosen so that if C subsumes D
 *   then feature(C) <= feature(D) componentwise.  The index retrieves exactly
 *   the clauses passing that test.
 *
 *   The vector used to be DENSE and positional -- two entries per relation
 *   symbol and four per function symbol in the whole input, for every clause
 *   -- and the index was a trie whose DEPTH was the position, so every
 *   clause's path spelled out its zeros as well as its values.  On a
 *   knowledge-base-sized input that is about 28,600 entries and 28,600 trie
 *   levels per clause, for a clause that mentions perhaps five symbols.
 *
 *   The vector is sparse now: only the non-zero entries, as (position, value)
 *   pairs.  Nodes carry their position explicitly, paths are as long as the
 *   clause is wide, and a node can hold both data and children because
 *   vectors no longer all have the same length.
 *
 * WHY THIS TEST IS SHAPED LIKE THIS
 *   The filter must retrieve the SAME SET as before.  It is allowed to be a
 *   conservative over-approximation of subsumption -- that is its job -- but
 *   if it retrieves a different set then different subsumption checks run and
 *   proofs change.  So the test does not assert what the author believes the
 *   traversal does; it compares the traversal against a brute-force scan over
 *   the dense vectors, on randomly generated data, including the cases that
 *   make sparse indexing delicate:
 *
 *     - vectors of different lengths, so a stored path ends where another
 *       continues (the case the old union of data/kids could not represent);
 *     - a stored vector that is all zeros, whose sparse form is EMPTY and
 *       which therefore sits at the root and is <= absolutely everything;
 *     - queries with zeros where stored vectors have values, which is what
 *       the pruning in di_walk_ge turns on;
 *     - duplicate vectors, which share a node.
 *
 *   di_tree_data_le and di_tree_data_ge exist for this: they are the filter
 *   without the subsumption test that normally follows it, and they walk with
 *   the same di_walk_le/di_walk_ge the provers use, so the thing measured
 *   here is the thing that runs.
 *
 * It runs in well under a second, which is why it is in tier1.
 */

#include "../ladr/di_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEN   12        /* dense vector length */
#define ITEMS 60        /* stored vectors */
#define TRIES 400       /* random queries per direction */

static int failures = 0;

static void check(const char *what, int ok)
{
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
}

static void report(const char *what)
{
  printf("ok:   %s\n", what);
}

/* A deliberately poor but completely reproducible generator, so a failure can
   be re-run.  Values stay small (0..3) so that collisions and ties are
   common, which is where indexing bugs live. */
static unsigned Seed = 12345;
static int rnd(int n)
{
  Seed = Seed * 1103515245u + 12345u;
  return (int) ((Seed >> 16) % (unsigned) n);
}

static int Dense[ITEMS][LEN];

static I2list sparse_of(const int *v, int len)
{
  I2list f = NULL, tail = NULL;
  int i;
  for (i = 0; i < len; i++)
    if (v[i] != 0) {
      I2list g = get_i2list();
      g->i = i;
      g->j = v[i];
      g->next = NULL;
      if (tail) tail->next = g; else f = g;
      tail = g;
    }
  return f;
}

static int dense_le(const int *a, const int *b, int len)
{
  int i;
  for (i = 0; i < len; i++)
    if (a[i] > b[i])
      return 0;
  return 1;
}

/* Is the datum for item i in the returned list? */
static int in_list(Plist p, int i)
{
  void *want = (void *) (long) (i + 1);
  for (; p; p = p->next)
    if (p->v == want)
      return 1;
  return 0;
}

int main(void)
{
  Di_tree root = init_di_tree();
  int i, j, t;
  int mismatches_le = 0, mismatches_ge = 0, counted = 0;

  /* ---- build the stored set ---- */
  for (i = 0; i < ITEMS; i++) {
    I2list f;
    /* Item 0 is deliberately all zeros: its sparse vector is empty, so it
       sits at the root and is <= every possible query.  The dense index could
       not have such a short path. */
    if (i == 0)
      memset(Dense[i], 0, sizeof(Dense[i]));
    else if (i == 1) {
      /* and one that is non-zero everywhere */
      for (j = 0; j < LEN; j++) Dense[i][j] = 3;
    }
    else if (i < 6) {
      /* a few that are non-zero in only one position, at different positions,
	 so their paths have length 1 and end at different depths */
      memset(Dense[i], 0, sizeof(Dense[i]));
      Dense[i][(i - 2) * 3 % LEN] = 1 + rnd(3);
    }
    else
      for (j = 0; j < LEN; j++)
	Dense[i][j] = rnd(4);

    f = sparse_of(Dense[i], LEN);
    di_tree_insert(f, root, (void *) (long) (i + 1));
    zap_i2list(f);
  }
  report("built an index over 60 vectors, including an all-zero one");

  /* ---- the <= direction, against brute force ---- */
  for (t = 0; t < TRIES; t++) {
    int q[LEN];
    I2list qf;
    Plist got;
    for (j = 0; j < LEN; j++)
      q[j] = rnd(4);
    /* every few queries, use a stored vector verbatim, so exact ties occur */
    if (t % 7 == 0)
      memcpy(q, Dense[t % ITEMS], sizeof(q));

    qf = sparse_of(q, LEN);
    got = di_tree_data_le(qf, root);

    for (i = 0; i < ITEMS; i++) {
      int want = dense_le(Dense[i], q, LEN);
      int have = in_list(got, i);
      counted++;
      if (want != have)
	mismatches_le++;
    }
    zap_plist(got);
    zap_i2list(qf);
  }
  check("di_tree_data_le agrees with a brute-force dense scan", mismatches_le == 0);
  if (mismatches_le == 0)
    report("di_tree_data_le agrees with brute force on 400 queries");

  /* ---- the >= direction ---- */
  for (t = 0; t < TRIES; t++) {
    int q[LEN];
    I2list qf;
    Plist got;
    for (j = 0; j < LEN; j++)
      q[j] = rnd(4);
    if (t % 5 == 0)
      memcpy(q, Dense[t % ITEMS], sizeof(q));
    /* and sometimes a query with many zeros, which is what exercises the
       "this branch skips a position I need" pruning */
    if (t % 11 == 0) {
      memset(q, 0, sizeof(q));
      q[rnd(LEN)] = 1 + rnd(3);
    }

    qf = sparse_of(q, LEN);
    got = di_tree_data_ge(qf, root);

    for (i = 0; i < ITEMS; i++) {
      int want = dense_le(q, Dense[i], LEN);
      int have = in_list(got, i);
      counted++;
      if (want != have)
	mismatches_ge++;
    }
    zap_plist(got);
    zap_i2list(qf);
  }
  check("di_tree_data_ge agrees with a brute-force dense scan", mismatches_ge == 0);
  if (mismatches_ge == 0)
    report("di_tree_data_ge agrees with brute force on 400 queries");

  printf("      %d membership decisions compared\n", counted);

  /* ---- features_less_or_equal, the same predicate standalone ---- */
  {
    int bad = 0;
    for (t = 0; t < 2000; t++) {
      int a[LEN], b[LEN];
      I2list fa, fb;
      for (j = 0; j < LEN; j++) { a[j] = rnd(3); b[j] = rnd(3); }
      fa = sparse_of(a, LEN);
      fb = sparse_of(b, LEN);
      if (features_less_or_equal(fa, fb) != (dense_le(a, b, LEN) ? TRUE : FALSE))
	bad++;
      zap_i2list(fa); zap_i2list(fb);
    }
    check("features_less_or_equal matches the dense predicate", bad == 0);
    if (!bad)
      report("features_less_or_equal matches the dense predicate, 2000 pairs");
  }

  /* ---- delete: removing everything must empty the index ---- */
  {
    Plist got;
    I2list qf;
    int q[LEN];
    for (i = 0; i < ITEMS; i++) {
      I2list f = sparse_of(Dense[i], LEN);
      di_tree_delete(f, root, (void *) (long) (i + 1));
      zap_i2list(f);
    }
    for (j = 0; j < LEN; j++)
      q[j] = 3;
    qf = sparse_of(q, LEN);
    got = di_tree_data_le(qf, root);
    check("after deleting every item the index returns nothing", got == NULL);
    if (got == NULL)
      report("delete removes every item, and prunes the nodes behind them");
    zap_plist(got);
    zap_i2list(qf);
    check("the index is pruned back to a bare root", root->kids == NULL);
    if (root->kids == NULL)
      report("the index is pruned back to a bare root");
  }

  zap_di_tree(root);

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures != 0;
}
