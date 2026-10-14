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

#ifndef TP_DI_TREE_H
#define TP_DI_TREE_H

#include "features.h"
#include "topform.h"

/* INTRODUCTION
*/

/* Public definitions */

typedef struct di_tree * Di_tree;

/* A node in a sparse feature-vector discrimination tree.
 *
 * Each node constrains ONE feature position to ONE value, so a stored clause
 * is the path from the root to the node holding it, and the path spells out
 * exactly that clause's non-zero entries.
 *
 * The dense version had only `label`, because depth WAS the position: the
 * tree was as deep as the feature vector was long -- around 28,600 levels on
 * a knowledge-base-sized input -- and every clause's path spelled out its
 * zeros as well as its values.  Positions are explicit now and zeros are
 * simply absent, so a path is as long as the clause is wide.
 *
 * `data` and `kids` can now BOTH be non-empty at one node: sparse vectors
 * have different lengths, so a clause's path can end where another's
 * continues.  That is why they are no longer a union.
 *
 * Siblings are kept ordered by (position, label), which is what lets both
 * traversals in di_tree.c stop early rather than examine every child.
 */

struct di_tree {
  int position;        /* which feature position this node constrains */
  int label;           /* the value required at that position */
  Di_tree next;        /* sibling, ordered by (position, label) */
  Di_tree kids;        /* children */
  Plist   data;        /* clauses whose vector ends at this node */
};

/* End of public definitions */

/* Public function prototypes from di_tree.c */

int nonunit_fsub_tests(void);

int nonunit_bsub_tests(void);

Di_tree get_di_tree(void);

void free_di_tree(Di_tree p);

void fprint_di_tree_mem(FILE *fp, BOOL heading);

void p_di_tree_mem(void);

Di_tree init_di_tree(void);

void di_tree_insert(I2list vec, Di_tree node, void *datum);

BOOL di_tree_delete(I2list vec, Di_tree node, void *datum);

void zap_di_tree(Di_tree node);

void p_di_tree(Di_tree node, int depth);

Topform forward_feature_subsume(Topform d, Di_tree root);

Plist back_feature_subsume(Topform c, Di_tree root);

/* The retrieval filter on its own, without the subsumption test that
   normally follows it: every datum whose stored vector is <= vec (or >= vec)
   componentwise.  These exist so that test.src/features_test.c can check the
   traversal against a brute-force scan; the provers use the two functions
   above, which walk with the same helpers. */

Plist di_tree_data_le(I2list vec, Di_tree root);

Plist di_tree_data_ge(I2list vec, Di_tree root);

unsigned mega_sub_calls(void);

#endif  /* conditional compilation of whole file */
