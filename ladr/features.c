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

#include "features.h"

/* Private definitions and types */

/* The featured symbols, in the order their entries appear in every feature
   vector.  This was an Ilist walked once per clause; it is a flat array now,
   for two reasons.  The walk is the inner loop of features() and runs over
   every distinct symbol in the whole input for every clause -- 7,161 symbols
   x 37,537 clauses on one real TPTP problem -- so pointer-chasing a list of
   one-int cells is the wrong shape; and the per-symbol "is this a function
   symbol" question, which decides whether a symbol contributes 2 entries or
   4, used to be asked inside that loop by calling function_symbol(), which
   goes to the symbol table.  It does not depend on the clause, so it is
   answered once here instead.  On the same problem that removed 821 million
   get_symbol_type calls and the bulk of 1.08 billion lookup_by_id calls. */

static int  Feature_count;     /* how many featured symbols */
static int *Feature_symbols;   /* their symnums, in vector order */
static char *Feature_isfunc;   /* 1 if that symbol is a function symbol */

/* The following are work arrays, indexed by symnum, used for calculating
   the features of a clause.  They are allocated by init_features() and
   left in place throughout the process.
*/

static int Work_size;         /* size of following arrays */
static int *Pos_occurrences;
static int *Neg_occurrences;
static int *Pos_maxdepth;
static int *Neg_maxdepth;

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
  int i;

  Work_size = greatest_symnum() + 1;

  /* printf("init_features: size=%d\n", Work_size); */

  /* Same symbols in the same order the Ilist had -- relation symbols then
     function symbols -- so every feature vector is laid out exactly as
     before. */
  Feature_count   = ilist_count(syms);
  Feature_symbols = calloc(Feature_count > 0 ? Feature_count : 1, sizeof(int));
  Feature_isfunc  = calloc(Feature_count > 0 ? Feature_count : 1, sizeof(char));
  for (p = syms, i = 0; p; p = p->next, i++) {
    Feature_symbols[i] = p->i;
    Feature_isfunc[i]  = function_symbol(p->i) ? 1 : 0;
  }
  zap_ilist(syms);

  Pos_occurrences = calloc(Work_size, sizeof(int));
  Neg_occurrences = calloc(Work_size, sizeof(int));
  Pos_maxdepth    = calloc(Work_size, sizeof(int));
  Neg_maxdepth    = calloc(Work_size, sizeof(int));
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
    else if (sign) {
      Pos_occurrences[sn]++;
      Pos_maxdepth[sn] = IMAX(depth, Pos_maxdepth[sn]);
    }
    else {
      Neg_occurrences[sn]++;
      Neg_maxdepth[sn] = IMAX(depth, Neg_maxdepth[sn]);
    }
    for (i = 0; i < ARITY(t); i++)
      fill_in_arrays(ARG(t,i), sign, depth+1);
  }
}  /* fill_in_arrays */

/*************
 *
 *   features()
 *
 *************/

/* DOCUMENTATION
Given a clause, build the feature vector.

Features:
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
*/

/* PUBLIC */
Ilist features(Literals lits)
{
  Ilist f = NULL, tail = NULL;
  Literals lit;
  int k;

  /* The original built the vector backwards with ilist_prepend and then
     called reverse_ilist on it.  Keeping the tail builds it forwards in the
     same order with the same number of cells and no reversing pass -- which
     at one call per clause was 7.69 s of a 171 s run on a 37,537-clause
     problem. */

#define APPEND_FEATURE(v)			\
  do {						\
    Ilist g_ = get_ilist();			\
    g_->i = (v);				\
    g_->next = NULL;				\
    if (tail) tail->next = g_; else f = g_;	\
    tail = g_;					\
  } while (0)

  APPEND_FEATURE(positive_literals(lits));
  APPEND_FEATURE(negative_literals(lits));

  for (lit = lits; lit; lit = lit->next) {
    fill_in_arrays(lit->atom, lit->sign, 0);
  }

  for (k = 0; k < Feature_count; k++) {
    int sn = Feature_symbols[k];

    APPEND_FEATURE(Pos_occurrences[sn]);
    APPEND_FEATURE(Neg_occurrences[sn]);

    if (Feature_isfunc[k]) {       /* was function_symbol(sn), per clause */
      APPEND_FEATURE(Pos_maxdepth[sn]);
      APPEND_FEATURE(Neg_maxdepth[sn]);
    }

    Pos_occurrences[sn] = 0;
    Neg_occurrences[sn] = 0;
    Pos_maxdepth[sn] = 0;
    Neg_maxdepth[sn] = 0;
  }

#undef APPEND_FEATURE

#if 0
  printf("Features for clause "); f_clause(c);
  p_features(f);
#endif
  return f;
}  /* features */

/*************
 *
 *   feature_length()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
int feature_length(void)
{
  int n = 2;  /* pos lits, neg lits */
  int k;
  for (k = 0; k < Feature_count; k++) {
    n += 2;
    if (Feature_isfunc[k])
      n += 2;
  }
  return n;
}  /* feature_length */

/*************
 *
 *   features_less_or_equal()
 *
 *************/

/* DOCUMENTATION
Return TRUE if Ilists c and d are thr same length and
each member of c is <= the corresponding member of d.
*/

/* PUBLIC */
BOOL features_less_or_equal(Ilist c, Ilist d)
{
  while (c && d && c->i <= d->i) {
    c = c->next;
    d = d->next;
  }
  return !c && !d;
}  /* features_less_or_equal */

/*************
 *
 *   p_features()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void p_features(Ilist f)
{
  int k;
  printf("  pos_lits=%d, neg_lits=%d\n", f->i, f->next->i);
  f = f->next->next;

  for (k = 0; k < Feature_count; k++) {
    printf("  symbol %s: ", sn_to_str(Feature_symbols[k]));
    printf("pos_occ=%d, neg_occ=%d", f->i, f->next->i);
    f = f->next->next;
    if (Feature_isfunc[k]) {
      printf(", pos_max=%d, neg_max=%d", f->i, f->next->i);
      f = f->next->next;
    }
    printf("\n");
  }
}  /* p_features */

