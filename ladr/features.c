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

        git diff 25b4505 -- ladr/features.c
*/

#include "features.h"

/* The subsumption feature vector, and why it is sparse.
 *
 * WHAT A FEATURE VECTOR IS FOR
 *   Subsumption is expensive, so Prover9 filters candidates first.  Each
 *   clause gets a vector of small non-negative integers with the property that
 *   if C subsumes D then feature(C) <= feature(D) componentwise.  The index in
 *   di_tree.c retrieves exactly the clauses passing that test, and only those
 *   go on to a real subsumption check.  The filter has to be sound -- never
 *   miss a subsumer -- and the cheaper it is the better.
 *
 * THE SHAPE PROBLEM
 *   The vector was DENSE and positional:
 *
 *       [ positive literals, negative literals,
 *         then for each relation symbol:  pos occurrences, neg occurrences,
 *         then for each function symbol:  pos occurrences, neg occurrences,
 *                                         pos maxdepth,    neg maxdepth ]
 *
 *   -- two entries per relation symbol and four per function symbol IN THE
 *   WHOLE INPUT, for every clause, built as an Ilist of one-int cells.
 *
 *   The length is therefore a property of the knowledge base and the
 *   information is a property of the clause.  On Problems/CSR/CSR040-10.p
 *   (44,227 clauses, 7,161 distinct symbols after translation) that is about
 *   28,600 entries per clause, nearly all of them zero, because a clause
 *   mentions perhaps five symbols.  Measured: 37,537 calls to features()
 *   produced 3.3 billion get_ilist/get_mem/free_mem cycles, around 27% of the
 *   whole run -- and on that problem the index they feed is EMPTY, because
 *   every clause is a unit and the nonunit index never receives an insert.
 *   di_tree_forward was called 37,537 times and cost 0.00 seconds, on vectors
 *   that took a quarter of the run to build and discard.
 *
 * THE SPARSE REPRESENTATION
 *   Only the non-zero entries, as (position, value) pairs in increasing
 *   position order: an I2list with ->i the position and ->j the value.  A
 *   clause mentioning k symbols has at most 2 + 4k pairs instead of 2 + 4R.
 *
 *   An absent position means zero, and that is what makes the filter work
 *   unchanged.  All entries are non-negative, so
 *
 *       feature(C) <= feature(D) componentwise
 *         iff  for every position p where C is non-zero, C[p] <= D[p]
 *
 *   because a position where C is zero constrains nothing.  The set of
 *   clauses the index retrieves is therefore IDENTICAL to the dense version's,
 *   which is why no proof changes.  See di_tree.c for the traversal.
 *
 *   Positions are assigned exactly as the dense layout assigned them, so a
 *   sparse vector is the dense vector with its zeros dropped -- which keeps
 *   feature_length() and p_features() meaningful and makes the two
 *   representations directly comparable in test.src/features_test.c.
 */

/* Private definitions and types */

/* The featured symbols, in the order their entries appear in a feature
   vector, with the information about each symbol that does not depend on the
   clause worked out once.

   This was an Ilist walked once per clause.  It is a flat array now, for two
   reasons.  The walk was the inner loop of features() and ran over every
   distinct symbol in the whole input for every clause, so pointer-chasing a
   list of one-int cells was the wrong shape.  And the per-symbol "is this a
   function symbol" question, which decides whether a symbol contributes two
   entries or four, used to be asked inside that loop by calling
   function_symbol(), which goes to the symbol table.  It does not depend on
   the clause, so it is answered once here.  On CSR040-10 that removed 821
   million get_symbol_type calls and the bulk of 1.08 billion lookup_by_id
   calls. */

static int   Feature_count;     /* how many featured symbols */
static int  *Feature_symbols;   /* their symnums, in vector order */
static char *Feature_isfunc;    /* 1 if that symbol is a function symbol */
static int  *Feature_base;      /* first vector position for that symbol */
static int   Feature_vec_length;/* notional length of a dense vector */

/* Reverse map: symnum -> index into the arrays above, or -1 if the symbol is
   not featured (it was created after init_features, or is not in fsyms or
   rsyms).  This is what lets features() go from "the symbols this clause
   mentions" to "the positions to emit", without scanning all of them. */

static int  *Feature_index;     /* indexed by symnum; -1 when not featured */

/* Work arrays, indexed by symnum, used for calculating the features of one
   clause.  They are allocated by init_features() and left in place.  They are
   zero between clauses: features() clears exactly the entries it set. */

static int Work_size;         /* size of following arrays */
static int *Pos_occurrences;
static int *Neg_occurrences;
static int *Pos_maxdepth;
static int *Neg_maxdepth;

/* The symbols the current clause mentions, collected by fill_in_arrays() so
   that features() need not look at any other symbol.  Sized by Work_size
   because that is the most distinct symbols a clause could possibly mention;
   Touched_n is tiny in practice. */

static int *Touched;          /* symnums, each at most once */
static int  Touched_n;
static char *Is_touched;      /* indexed by symnum, to keep Touched distinct */

/*************
 *
 *   init_features()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void init_features(Ilist fsyms, Ilist rsyms)
{
  Ilist syms = ilist_cat(ilist_copy(rsyms), ilist_copy(fsyms));
  Ilist p;
  int i, pos;

  Work_size = greatest_symnum() + 1;

  /* Same symbols in the same order the Ilist had -- relation symbols, then
     function symbols -- so positions are exactly the dense layout's. */
  Feature_count   = ilist_count(syms);
  Feature_symbols = calloc(Feature_count > 0 ? Feature_count : 1, sizeof(int));
  Feature_isfunc  = calloc(Feature_count > 0 ? Feature_count : 1, sizeof(char));
  Feature_base    = calloc(Feature_count > 0 ? Feature_count : 1, sizeof(int));

  Feature_index = calloc(Work_size, sizeof(int));
  for (i = 0; i < Work_size; i++)
    Feature_index[i] = -1;

  pos = 2;  /* positions 0 and 1 are the literal counts */
  for (p = syms, i = 0; p; p = p->next, i++) {
    Feature_symbols[i] = p->i;
    Feature_isfunc[i]  = function_symbol(p->i) ? 1 : 0;
    Feature_base[i]    = pos;
    pos += Feature_isfunc[i] ? 4 : 2;
    if (p->i >= 0 && p->i < Work_size)
      Feature_index[p->i] = i;
  }
  Feature_vec_length = pos;
  zap_ilist(syms);

  Pos_occurrences = calloc(Work_size, sizeof(int));
  Neg_occurrences = calloc(Work_size, sizeof(int));
  Pos_maxdepth    = calloc(Work_size, sizeof(int));
  Neg_maxdepth    = calloc(Work_size, sizeof(int));

  Touched    = calloc(Work_size > 0 ? Work_size : 1, sizeof(int));
  Is_touched = calloc(Work_size > 0 ? Work_size : 1, sizeof(char));
  Touched_n  = 0;
}  /* init_features */

/*************
 *
 *   fill_in_arrays()
 *
 *************/

static
void fill_in_arrays(Term t, BOOL sign, int depth)
{
  if (!VARIABLE(t)) {
    int sn = SYMNUM(t);
    int i;
    if (sn >= Work_size) {
      /* Assume it's a symbol that was added after the start of
	 the search.  If we ignore symbols in features, we may get
	 less discrimination, but all answers should be returned.
       */
      ;
    }
    else {
      if (!Is_touched[sn]) {        /* remember it, once */
	Is_touched[sn] = 1;
	Touched[Touched_n++] = sn;
      }
      if (sign) {
	Pos_occurrences[sn]++;
	Pos_maxdepth[sn] = IMAX(depth, Pos_maxdepth[sn]);
      }
      else {
	Neg_occurrences[sn]++;
	Neg_maxdepth[sn] = IMAX(depth, Neg_maxdepth[sn]);
      }
    }
    for (i = 0; i < ARITY(t); i++)
      fill_in_arrays(ARG(t,i), sign, depth+1);
  }
}  /* fill_in_arrays */

/*************
 *
 *   compare_feature_index()
 *
 *************/

/* Order touched symbols by their position in the vector.  Feature_index is a
   strictly increasing function of position, so comparing indexes is the same
   as comparing positions. */

static
int compare_feature_index(const void *a, const void *b)
{
  int ia = Feature_index[*(const int *) a];
  int ib = Feature_index[*(const int *) b];
  return ia < ib ? -1 : (ia > ib ? 1 : 0);
}  /* compare_feature_index */

/*************
 *
 *   features()
 *
 *************/

/* DOCUMENTATION
Given a clause, build its sparse feature vector: the non-zero entries of

  positive literals
  negative literals
  foreach relation symbol
     positive occurrences
     negative occurrences
  foreach function symbol
     positive occurrences
     negative occurrences
     positive maxdepth
     negative maxdepth

as (position, value) pairs in increasing position order.  An absent position
means zero.  Free the result with zap_i2list().
*/

/* PUBLIC */
I2list features(Literals lits)
{
  I2list f = NULL, tail = NULL;
  Literals lit;
  int n, k;

  /* Append (pos, val) at the tail, skipping zeros.  Building forwards with a
     tail pointer rather than prepending and reversing: same cells, same
     order, no reversing pass. */
#define EMIT(position, value)			\
  do {						\
    int v_ = (value);				\
    if (v_ != 0) {				\
      I2list g_ = get_i2list();			\
      g_->i = (position);			\
      g_->j = v_;				\
      g_->next = NULL;				\
      if (tail) tail->next = g_; else f = g_;	\
      tail = g_;				\
    }						\
  } while (0)

  EMIT(0, positive_literals(lits));
  EMIT(1, negative_literals(lits));

  Touched_n = 0;
  for (lit = lits; lit; lit = lit->next)
    fill_in_arrays(lit->atom, lit->sign, 0);

  /* The clause's symbols come out of fill_in_arrays in the order encountered,
     which is not vector order; the index requires increasing positions.
     Touched_n is the number of distinct symbols in ONE clause, so this sort
     is over a handful of elements -- the dense version's cost was a walk over
     every symbol in the input instead. */
  if (Touched_n > 1)
    qsort(Touched, Touched_n, sizeof(int), compare_feature_index);

  for (n = 0; n < Touched_n; n++) {
    int sn = Touched[n];
    k = Feature_index[sn];

    if (k >= 0) {                 /* a featured symbol: emit its entries */
      int base = Feature_base[k];
      EMIT(base + 0, Pos_occurrences[sn]);
      EMIT(base + 1, Neg_occurrences[sn]);
      if (Feature_isfunc[k]) {
	EMIT(base + 2, Pos_maxdepth[sn]);
	EMIT(base + 3, Neg_maxdepth[sn]);
      }
    }

    /* Clear what we set, whether or not the symbol was featured, so the work
       arrays are zero for the next clause.  The dense version cleared only
       the featured entries; a non-featured symbol with a symnum below
       Work_size would have left a value behind.  It was never read, because
       the dense loop only looked at featured positions, so this is the same
       behaviour made local rather than a fix. */
    Pos_occurrences[sn] = 0;
    Neg_occurrences[sn] = 0;
    Pos_maxdepth[sn] = 0;
    Neg_maxdepth[sn] = 0;
    Is_touched[sn] = 0;
  }
  Touched_n = 0;

#undef EMIT

  return f;
}  /* features */

/*************
 *
 *   feature_length()
 *
 *************/

/* DOCUMENTATION
The length a dense feature vector would have: two for the literal counts, then
two per relation symbol and four per function symbol.  The index no longer
needs this -- a sparse vector carries its own positions -- but it states the
size of the space the positions live in.
*/

/* PUBLIC */
int feature_length(void)
{
  return Feature_vec_length;
}  /* feature_length */

/*************
 *
 *   features_less_or_equal()
 *
 *************/

/* DOCUMENTATION
Is feature vector c <= feature vector d, componentwise, treating an absent
position as zero?  Both must be in increasing position order.

Every entry is non-negative, so a position where c is zero constrains nothing
and a position where c is non-zero and d is absent fails.
*/

/* PUBLIC */
BOOL features_less_or_equal(I2list c, I2list d)
{
  while (c) {
    while (d && d->i < c->i)       /* d has an entry c does not: no constraint */
      d = d->next;
    if (d == NULL || d->i > c->i)  /* d is zero where c is not */
      return FALSE;
    if (c->j > d->j)
      return FALSE;
    c = c->next;
    d = d->next;
  }
  return TRUE;
}  /* features_less_or_equal */

/*************
 *
 *   p_features()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void p_features(I2list f)
{
  int k;
  int pos_lits = (f && f->i == 0) ? f->j : 0;
  int neg_lits = 0;
  I2list p;

  for (p = f; p && p->i <= 1; p = p->next)
    if (p->i == 1)
      neg_lits = p->j;

  printf("  pos_lits=%d, neg_lits=%d\n", pos_lits, neg_lits);

  for (k = 0; k < Feature_count; k++) {
    int base = Feature_base[k];
    int width = Feature_isfunc[k] ? 4 : 2;
    int v[4] = { 0, 0, 0, 0 };
    int any = 0;
    for (p = f; p; p = p->next)
      if (p->i >= base && p->i < base + width) {
	v[p->i - base] = p->j;
	any = 1;
      }
    if (!any)
      continue;                    /* absent means all zero; say nothing */
    printf("  symbol %s: ", sn_to_str(Feature_symbols[k]));
    printf("pos_occ=%d, neg_occ=%d", v[0], v[1]);
    if (Feature_isfunc[k])
      printf(", pos_max=%d, neg_max=%d", v[2], v[3]);
    printf("\n");
  }
}  /* p_features */
