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

#include "unfold.h"

/* Private definitions and types */

struct symdata {
  Term alpha, beta;
  Ilist dependents;
  Ilist dependents_tail;  /* last cell of dependents, so appending is O(1) */
  BOOL listed;            /* already in the `symbols` list */
  int status;
};

/*************
 *
 *   trace_dependents()
 *
 *************/

enum {NOT_CHECKED = 0, CHECKING, CHECKED, CYCLE};

static
void trace_dependents(int i, struct symdata *table)
{
  if (table[i].status == CHECKED)
    return;
  else if (table[i].status == CHECKING) {
    table[i].status = CYCLE;
    return;
  }
  else if (table[i].status == CYCLE)
    return;
  else {
    Ilist p;
    BOOL ok = TRUE;
    table[i].status = CHECKING;
    for (p = table[i].dependents; p; p = p->next) {
      trace_dependents(p->i, table);
      if (table[p->i].status == CYCLE)
	ok = FALSE;
    }
    
    table[i].status = (ok ? CHECKED : CYCLE);
    return;
  }
}  /* trace_dependents */

/*************
 *
 *   eliminate_cycles()
 *
 *************/

/* Drop every symbol whose status is CYCLE, in place, freeing its cell and its
   dependent list.

   This was recursive, one frame per symbol: it fixed up the tail before
   testing the head, which reads neatly but makes the stack depth the symbol
   count.  See PF5 in ../FIXES.md for why that is a crash rather than a style
   question.  A forward filter removes the same symbols in the same order. */

static
Ilist eliminate_cycles(Ilist symbols, struct symdata *table)
{
  Ilist p, prev, next;
  for (p = symbols, prev = NULL; p; p = next) {
    next = p->next;
    if (table[p->i].status == CYCLE) {
      int i = p->i;
      if (prev)
	prev->next = next;
      else
	symbols = next;
      free_ilist(p);
      zap_ilist(table[i].dependents);
      table[i].dependents = NULL;
      table[i].dependents_tail = NULL;
    }
    else
      prev = p;
  }
  return symbols;
}  /* eliminate_cycles */

/*************
 *
 *   sym_less_or_equal()
 *
 *************/

static
BOOL sym_less_or_equal(int s1, int s2, struct symdata *table)
{
  if (s1 == s2)
    return TRUE;
  else {
    Ilist p;
    for (p = table[s2].dependents; p; p = p->next)
      if (sym_less_or_equal(s1, p->i, table))
	return TRUE;
    return FALSE;
  }
}  /* sym_less_or_equal */

/*************
 *
 *   compare_symbols()
 *
 *************/

static
Ordertype compare_symbols(int s1, int s2, struct symdata *table)
{
  if (s1 == s2)
    return SAME_AS;
  else if (sym_less_or_equal(s1, s2, table))
    return LESS_THAN;
  else if (sym_less_or_equal(s2, s1, table))
    return GREATER_THAN;
  else if (sn_to_arity(s1) < sn_to_arity(s2))
    return LESS_THAN;
  else if (sn_to_arity(s2) < sn_to_arity(s1))
    return GREATER_THAN;
  else {
    int i = strcmp(sn_to_str(s1), sn_to_str(s2));
    if (i < 0)
      return LESS_THAN;
    else if (i > 0)
      return GREATER_THAN;
    else
      return SAME_AS;
  }
}  /* compare_symbols */

/*************
 *
 *   insert_symbol()
 *
 *************/

/* Insert sym before the first element it is not GREATER_THAN.  Was recursive,
   one frame per element skipped; the comparison sequence and the insertion
   point are unchanged. */

static
Ilist insert_symbol(Ilist syms, int sym, struct symdata *table)
{
  Ilist p, prev, g;
  for (p = syms, prev = NULL; p; prev = p, p = p->next)
    if (compare_symbols(sym, p->i, table) != GREATER_THAN)
      break;
  g = get_ilist();
  g->i = sym;
  g->next = p;
  if (prev == NULL)
    return g;
  prev->next = g;
  return syms;
}  /* insert_symbol */

/*************
 *
 *   order_symbols()
 *
 *************/

static
Ilist order_symbols(Ilist syms, struct symdata *table)
{
  Ilist new = NULL;
  Ilist p;
  for (p = syms; p; p = p->next)
    new = insert_symbol(new, p->i, table);
  zap_ilist(syms);
  return new;
}  /* order_symbols */

/*************
 *
 *   eq_defs()
 *
 *************/

static
Ilist eq_defs(Clist clauses, int arity_limit)
{
  Ilist symbols = NULL, symbols_tail = NULL;
  Ilist p;
  int size = greatest_symnum() + 1;
  int i;
  struct symdata *table = calloc(size, sizeof(struct symdata));
  Clist_pos cp;

  /* Three costs in this loop, all of them a walk over something that the loop
     itself is making longer, and all of them measured on
     Problems/CSR/CSR040-10.p, which has 22,510 equational definitions:

       - ilist_cat() finds the tail of table[symbol].dependents by walking it.
	 Definitions of one symbol accumulate there, so the walk grows with
	 every definition: 22,512 calls, 15.57 s, and 51% of the whole run
	 once the feature-vector work in PF4 was done.  A tail pointer makes
	 it O(1).

       - ilist_member(symbols, symbol) scans the list of symbols already seen,
	 once per definition.  table[symbol].listed answers the same question
	 by index.

       - ilist_append(symbols, symbol) then walks that list again to reach its
	 tail.  symbols_tail removes that walk.

     None of this changes the result.  dependents keeps the same elements in
     the same order, duplicates across definitions included -- the original
     concatenated sets without merging them and so had those duplicates too --
     and symbols keeps each symbol once, in first-seen order. */

  for (cp = clauses->first; cp; cp = cp->next) {
    Topform c = cp->c;
    int rc = equational_def(c);  /* 0=no; 1=LR, 2=RL */
    if (rc != 0) {
      Term alpha = ARG(c->literals->atom, (rc == 1 ? 0 : 1));
      Term beta  = ARG(c->literals->atom, (rc == 1 ? 1 : 0));
      if (ARITY(alpha) <= arity_limit && ARITY(beta) > 0) {
	int symbol = SYMNUM(alpha);
	Ilist new_deps = fsym_set_in_term(beta);

	if (new_deps != NULL) {
	  if (table[symbol].dependents_tail)
	    table[symbol].dependents_tail->next = new_deps;
	  else
	    table[symbol].dependents = new_deps;
	  table[symbol].dependents_tail = ilist_last(new_deps);
	}

	table[symbol].alpha = alpha;
	table[symbol].beta = beta;

	if (!table[symbol].listed) {
	  Ilist g = get_ilist();
	  g->i = symbol;
	  g->next = NULL;
	  if (symbols_tail)
	    symbols_tail->next = g;
	  else
	    symbols = g;
	  symbols_tail = g;
	  table[symbol].listed = TRUE;
	}
      }
    }
  }

  // trace dependencies (in table)

  for (p = symbols; p; p = p->next)
    trace_dependents(p->i, table);

  // eliminate symbols involved in cycles

  symbols = eliminate_cycles(symbols, table);

  // partial-order -> total-order (by partial-order, arity, strcmp)

  symbols = order_symbols(symbols, table);

  for (i = 0; i < size; i++)
    zap_ilist(table[i].dependents);
  free(table);

  return symbols;
}  /* eq_defs */

/*************
 *
 *   num_constant_symbols()
 *
 *************/

static
int num_constant_symbols(Ilist p)
{
  if (p == NULL)
    return 0;
  else
    return (sn_to_arity(p->i) == 0 ? 1 : 0) + num_constant_symbols(p->next);
}  /* num_constant_symbols */

/*************
 *
 *   constant_check()
 *
 *************/

static
BOOL constant_check(int symnum, Ilist symbols, Clist clauses, int constant_limit)
{
  if (sn_to_arity(symnum) > 0)
    return TRUE;
  else if (num_constant_symbols(symbols) > constant_limit)
    return FALSE;
  else {
    /* ok if the constant occurs in a negative clause */
    Clist_pos cp;
    for (cp = clauses->first; cp; cp = cp->next) {
      Topform c = cp->c;
      if (negative_clause(c->literals)) {
	Literals lit;
	for (lit = cp->c->literals; lit; lit = lit->next) {
	  if (symbol_in_term(symnum, lit->atom))
	    return TRUE;
	}
      }
    }
    return FALSE;
  }
}  /* constant_check */

/*************
 *
 *   unfold_eq_defs()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void unfold_eq_defs(Clist clauses, int arity_limit, int constant_limit, BOOL print)
{
  Ilist symbols = eq_defs(clauses, arity_limit);
  Ilist p;
  int n = 0;

  // Now we have a list of symbols that can be unfolded.  There can
  // be dependencies, but there are no cycles.  Dependents are earlier.
  // If we were always using LPO or RPO, we could simply give these
  // symbols highest precedence (in the same order).  However,
  // for KBO we want to be able to unfold when there are repeated
  // variables in the right side, e.g., g(x) = f(x,x), which does not
  // satisfy KBO.  Therefore, we do not control unfolding by setting
  // precedences and KBO weights.  Instead, we flag the symbol as "unfold"
  // and the routine that orients equalities checks for that special case.

  if (print)
    printf("Unfolding symbols:");
  for (p = symbols; p; p = p->next) {
    int i = p->i;
    if (constant_check(i, symbols, clauses, constant_limit)) {
      n++;
      // assign_greatest_precedence(i);   /* for LRPO */
      // set_kb_weight(SYMNUM(table[i].alpha), kbo_weight(table[i].beta) + 1);
      set_unfold_symbol(i);
      if (print)
	printf(" %s/%d", sn_to_str(i), sn_to_arity(i));
    }
  }
  if (print)
    printf("%s\n", n > 0 ? "." : " (none).");

  zap_ilist(symbols);
}  /* unfold_eq_defs */

/*************
 *
 *   remove_kb_wt_zero()
 *
 *************/

static
Ilist remove_kb_wt_zero(Ilist syms)
{
  if (syms == NULL)
    return NULL;
  else {
    syms->next = remove_kb_wt_zero(syms->next);
    if (sn_to_kb_wt(syms->i) == 0) {
      Ilist next = syms->next;
      free_ilist(syms);
      return next;
    }
    else
      return syms;
  }
}  /* remove_kb_wt_zero */

/*************
 *
 *   fold_eq_defs()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL fold_eq_defs(Clist clauses, BOOL kbo)
{
  Ilist symbols = eq_defs(clauses, INT_MAX);
  Ilist p;
  BOOL change;

  if (kbo)
    /* required for termination */
    symbols = remove_kb_wt_zero(symbols);

  printf("Folding symbols:");
  for (p = symbols; p; p = p->next)
    printf(" %s/%d", sn_to_str(p->i), sn_to_arity(p->i));
  printf("%s\n", symbols ? "." : " (none).");

  lex_insert_after_initial_constants(symbols);
  change = (symbols != NULL);
  zap_ilist(symbols);
  return change;
}  /* fold_eq_defs */

/*************
 *
 *   one_unary_def()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL one_unary_def(Clist a, Clist b)
{
  Ilist d1 = eq_defs(a, 1);
  Ilist d2 = eq_defs(b, 1);
  BOOL rc = (ilist_count(d1) + ilist_count(d2) == 1);
  zap_ilist(d1);
  zap_ilist(d2);
  return rc;
}  /* one_unary_def */

