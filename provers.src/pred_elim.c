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

#include "pred_elim.h"

/* Private definitions and types */

/* The last cell of a Plist, or NULL for the empty list.  plist_last() returns
   the last VALUE; the queue in elim_relation() needs the cell. */

static
Plist plist_last_cell(Plist p)
{
  if (p == NULL)
    return NULL;
  while (p->next)
    p = p->next;
  return p;
}  /* plist_last_cell */

/*************
 *
 *   rsym_occurrences() - number of occurrences of a pred symbol in a clause
 *
 *************/

static
int rsym_occurrences(int symbol, Topform c)
{
  int n = 0;
  Literals lit;
  for (lit = c->literals; lit; lit = lit->next)
    if (SYMNUM(lit->atom) == symbol)
      n++;
  return n;
}  /* rsym_occurrences */

/*************
 *
 *   arg_check()
 *
 *************/

static
BOOL arg_check(int symbol, Topform c)
{
  Literals lit;
  for (lit = c->literals; lit; lit = lit->next) {
    if (!lit->sign &&
	SYMNUM(lit->atom) == symbol &&
	!args_distinct_vars(lit->atom))
      return FALSE;
  }
  return TRUE;
}  /* arg_check */

/*************
 *
 *   eliminable_relation()
 *
 *************/

/* This used to be eliminable_relation(symbol, clauses, equality), called once
   per relation symbol by eliminable_relations() below, and each call scanned
   every clause and every literal.  Once per symbol over all symbols is
   O(R * L), and the caller then ran that O(R * L) scan once per symbol
   ELIMINATED, which made predicate elimination O(R^2 * L) -- cubic for a file
   whose clause count grows with its predicate count.  At 3,000 predicates it
   was 37.6% of the whole run in a profile, and startup took 68.58 s against
   1.27 s with predicate_elim cleared.

   What the predicate actually says, per symbol, is:

     - no single clause contains more than one occurrence of it, and
     - (under equality) no negative occurrence of it has arguments that are
       not distinct variables

   and ONE pass over the clauses can decide that for every symbol at once.
   That is what gather_eliminability() does: O(L) rather than O(R * L), with
   identical results, because it computes exactly the same two facts.

   The counts go in an I2accum (see glist.c), the hash-backed multiset added
   for PF2, so each symbol's running maximum is an O(1) lookup rather than a
   list walk. */

static
void gather_eliminability(Plist clauses, BOOL equality,
			  I2accum maxocc, I2accum bad)
{
  Plist p;
  for (p = clauses; p; p = p->next) {
    Topform c = p->v;
    Literals lit;
    for (lit = c->literals; lit; lit = lit->next) {
      int sym = SYMNUM(lit->atom);
      int n = rsym_occurrences(sym, c);   /* O(literals in this clause) */
      int cur = i2accum_occurrences(maxocc, sym);
      if (n > cur)
	i2accum_add_n(maxocc, sym, n - cur);   /* keep the running maximum */
      if (equality && !lit->sign && !args_distinct_vars(lit->atom))
	i2accum_add(bad, sym);
    }
  }
}  /* gather_eliminability */

/*************
 *
 *   eliminable_relations()
 *
 *************/

static
Ilist eliminable_relations(Plist clauses, BOOL equality)
{
  Ilist rsyms = rsym_set_in_topforms(clauses);
  I2accum maxocc = get_i2accum();
  I2accum bad = get_i2accum();
  Ilist eliminable = NULL, tail = NULL;
  Ilist p;

  gather_eliminability(clauses, equality, maxocc, bad);

  for (p = rsyms; p; p = p->next) {
    if (!is_eq_symbol(p->i) &&               /* don't allow equalities */
	sn_to_arity(p->i) != 0 &&            /* don't allow prop atoms */
	i2accum_occurrences(maxocc, p->i) <= 1 &&
	(!equality || i2accum_occurrences(bad, p->i) == 0)) {
      /* ilist_append walks to the tail, so appending in a loop is quadratic
	 in the number of eliminable symbols; keep the tail instead.  rsyms is
	 traversed in the same order as before, so the result list is the same
	 list, which matters because the caller uses its FIRST element. */
      Ilist g = get_ilist();
      g->i = p->i;
      g->next = NULL;
      if (tail)
	tail->next = g;
      else
	eliminable = g;
      tail = g;
    }
  }
  zap_ilist(rsyms);
  zap_i2accum(maxocc);
  zap_i2accum(bad);
  return eliminable;
}  /* eliminable_relations */

/*************
 *
 *   resolve_on_symbol()
 *
 *************/

static
Plist resolve_on_symbol(int symbol, Topform c1, Topform c2)
{
  Plist resolvents = NULL;
  Literals l1, l2;
  for (l1 = c1->literals; l1; l1 = l1->next)
    for (l2 = c2->literals; l2; l2 = l2->next) {
      if (l1->sign != l2->sign &&
	  SYMNUM(l1->atom) == symbol &&
	  SYMNUM(l2->atom) == symbol) {
	Topform res;
	if (c1->id == 0) {
	  assign_clause_id(c1);
	  fwrite_clause(stdout, c1, CL_FORM_STD);
	}
	if (c2->id == 0) {
	  assign_clause_id(c2);
	  fwrite_clause(stdout, c2, CL_FORM_STD);
	}
	res = resolve3(c1, l1, c2, l2, TRUE);
	if (res) {
	  if (tautology(res->literals))
	    delete_clause(res);
	  else {
	    resolvents = plist_append(resolvents, res);
	  }
	}
      }
    }
  return resolvents;
}  /* resolve_on_symbol */

/*************
 *
 *   gen_given()
 *
 *************/

static
Plist gen_given(int symbol, Topform given, Plist usable)
{
  Plist new = NULL;
  Plist p;

  /* resolve_on_symbol() requires a literal with this symnum in BOTH clauses,
     so a clause that does not contain the symbol yields nothing -- including
     no clause-id assignment, since that happens inside the same test.
     Skipping those is therefore result-preserving, and it is what turns the
     all-pairs scan into a scan over only the clauses that mention the symbol.
     Everything still goes into `usable` below; only resolution is skipped. */
  if (rsym_occurrences(symbol, given) == 0)
    return NULL;

  for (p = usable; p; p = p->next) {
    Topform c = p->v;
    Plist resolvents;
    if (rsym_occurrences(symbol, c) == 0)
      continue;
    resolvents = resolve_on_symbol(symbol, given, c);
    new = plist_cat(new, resolvents);
  }
  return new;
}  /* gen_given */

/*************
 *
 *   subsumed_by_member()
 *
 *************/

static
BOOL subsumed_by_member(Topform c, Plist p)
{
  /* Was recursive, one frame per clause in sos; see PF5 in ../FIXES.md for
     why that is a crash and not just a style. */
  Plist q;
  for (q = p; q; q = q->next)
    if (subsumes(q->v, c))
      return TRUE;
  return FALSE;
}  /* subsumed_by_member */

/*************
 *
 *   incorporate_new_clauses()
 *
 *************/

/* sos is a queue: elim_relation() pops the head and appends at the tail.
   plist_append() walks to the tail, so appending once per derived clause cost
   O(|sos|) each time and the loop in elim_relation() was quadratic in the
   clause count -- 9,000,012 plist_append calls and 22.4% of the profile at
   3,000 predicates.  Threading the tail pointer through makes each append
   O(1).  Nothing else changes: the subsumption test still sees the clauses
   appended earlier in this same call, exactly as it did when sos was
   reassigned each time. */

static
void incorporate_new_clauses(Plist *sos, Plist *sos_tail, Plist new, BOOL echo)
{
  Plist p;
  for (p = new; p; p = p->next) {
    if (!subsumed_by_member(p->v, *sos)) {
      Plist g = get_plist();
      g->v = p->v;
      g->next = NULL;
      if (*sos_tail)
	(*sos_tail)->next = g;
      else
	*sos = g;
      *sos_tail = g;
      printf("Derived: ");
      fwrite_clause(stdout, p->v, CL_FORM_STD);
    }
  }
  zap_plist(new);
}  /* incorporate_new_clauses */

/*************
 *
 *   elim_relation()
 *
 *************/

static
Plist elim_relation(int symbol, Plist sos, Clist disabled, BOOL echo)
{
  /* this does a naive given-clause loop */
  Plist usable = NULL, usable_tail = NULL;
  Plist sos_tail = plist_last_cell(sos);
  while (sos) {
    Plist new;
    Topform given = sos->v;
    if (sos->next == NULL)
      sos_tail = NULL;          /* popping the last cell empties the queue */
    sos = plist_pop(sos);
    {
      /* usable = plist_append(usable, given), with the tail kept: the walk
	 this replaces made the loop quadratic in the clause count. */
      Plist g = get_plist();
      g->v = given;
      g->next = NULL;
      if (usable_tail)
	usable_tail->next = g;
      else
	usable = g;
      usable_tail = g;
    }
#ifdef GEN_DEBUG
    printf("\ngiven: "); f_clause(given);
#endif
    new = gen_given(symbol, given, usable);
    incorporate_new_clauses(&sos, &sos_tail, new, echo);
  }
  /* partition usable into clauses with and without symbol */
  {
    Plist without = NULL, without_tail = NULL;
    Plist p;
    for (p = usable; p; p = p->next) {
      if (rsym_occurrences(symbol, p->v) > 0)
	clist_append(p->v, disabled);
      else {
	Plist g = get_plist();
	g->v = p->v;
	g->next = NULL;
	if (without_tail)
	  without_tail->next = g;
	else
	  without = g;
	without_tail = g;
      }
    }
    zap_plist(usable);
    return without;
  }
}  /* elim_relation */

/*************
 *
 *   predicate_elimination()
 *
 *************/

/* DOCUMENTATION
Apply predicate elimination to a Clist of clauses.  Clauses that are
eliminated are moved to the disabled list, and new clauses are appended
to the list of clauses.  Any new clauses will have binary resolution
justifications.
<p>
The initial clauses should not have IDs.  All clauses that
are used for resolution are given IDs, but the clauses in
the end result do not have IDs.
*/

/* PUBLIC */
void predicate_elimination(Clist clauses, Clist disabled, BOOL echo)
{
  Plist clauses2 = prepend_clist_to_plist(NULL, clauses);
  BOOL equality = equality_in_clauses(clauses2);  /* eq => different method */
  Ilist syms = eliminable_relations(clauses2, equality);

  if (syms == NULL) {
    zap_plist(clauses2);
    if (echo)
      printf("\nNo predicates eliminated.\n");
  }
  else {
    clist_remove_all_clauses(clauses);
    while (syms) {
      /* use first symbol, discard rest, get new list */
      if (echo)
	  printf("\nEliminating %s/%d\n",
		 sn_to_str(syms->i),
		 sn_to_arity(syms->i));
	
      clauses2 = elim_relation(syms->i, clauses2, disabled, echo);
      zap_ilist(syms);
      syms = eliminable_relations(clauses2, equality);
    }
    clist_append_plist(clauses, clauses2);
    zap_plist(clauses2);
  }
}  /* predicate_elimination */

