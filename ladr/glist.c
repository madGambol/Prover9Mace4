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

#include "glist.h"

/*
 * memory management
 */

#define PTRS_ILIST PTRS(sizeof(struct ilist))
static unsigned Ilist_gets, Ilist_frees;

#define PTRS_PLIST PTRS(sizeof(struct plist))
static unsigned Plist_gets, Plist_frees;

#define PTRS_I2LIST PTRS(sizeof(struct i2list))
static unsigned I2list_gets, I2list_frees;

#define PTRS_I3LIST PTRS(sizeof(struct i3list))
static unsigned I3list_gets, I3list_frees;

/*************
 *
 *   Ilist get_ilist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Ilist get_ilist(void)
{
  Ilist p = get_mem(PTRS_ILIST);
  p->next = NULL;
  Ilist_gets++;
  return(p);
}  /* get_ilist */

/*************
 *
 *    free_ilist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_ilist(Ilist p)
{
  free_mem(p, PTRS_ILIST);
  Ilist_frees++;
}  /* free_ilist */

/*************
 *
 *   Plist get_plist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Plist get_plist(void)
{
  Plist p = get_mem(PTRS_PLIST);
  p->next = NULL;
  Plist_gets++;
  return(p);
}  /* get_plist */

/*************
 *
 *    free_plist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_plist(Plist p)
{
  free_mem(p, PTRS_PLIST);
  Plist_frees++;
}  /* free_plist */

/*************
 *
 *   I2list get_i2list()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
I2list get_i2list(void)
{
  I2list p = get_mem(PTRS_I2LIST);
  p->next = NULL;
  I2list_gets++;
  return(p);
}  /* get_i2list */

/*************
 *
 *    free_i2list()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_i2list(I2list p)
{
  free_mem(p, PTRS_I2LIST);
  I2list_frees++;
}  /* free_i2list */

/*************
 *
 *   I3list get_i3list()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
I3list get_i3list(void)
{
  I3list p = get_mem(PTRS_I3LIST);
  p->next = NULL;
  I3list_gets++;
  return(p);
}  /* get_i3list */

/*************
 *
 *    free_i3list()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void free_i3list(I3list p)
{
  free_mem(p, PTRS_I3LIST);
  I3list_frees++;
}  /* free_i3list */

/*************
 *
 *   fprint_glist_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to FILE *fp) memory usage statistics for data types
associated with the glist package.
The Boolean argument heading tells whether to print a heading on the table.
*/

/* PUBLIC */
void fprint_glist_mem(FILE *fp, BOOL heading)
{
  int n;
  if (heading)
    fprintf(fp, "  type (bytes each)        gets      frees     in use      bytes\n");

  n = sizeof(struct ilist);
  fprintf(fp, "ilist (%4d)        %11u%11u%11u%9.1f K\n",
          n, Ilist_gets, Ilist_frees,
          Ilist_gets - Ilist_frees,
          ((Ilist_gets - Ilist_frees) * n) / 1024.);

  n = sizeof(struct plist);
  fprintf(fp, "plist (%4d)        %11u%11u%11u%9.1f K\n",
          n, Plist_gets, Plist_frees,
          Plist_gets - Plist_frees,
          ((Plist_gets - Plist_frees) * n) / 1024.);

  n = sizeof(struct i2list);
  fprintf(fp, "i2list (%4d)       %11u%11u%11u%9.1f K\n",
          n, I2list_gets, I2list_frees,
          I2list_gets - I2list_frees,
          ((I2list_gets - I2list_frees) * n) / 1024.);

}  /* fprint_glist_mem */

/*************
 *
 *   p_glist_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to stdout) memory usage statistics for data types
associated with the glist package.
*/

/* PUBLIC */
void p_glist_mem()
{
  fprint_glist_mem(stdout, TRUE);
}  /* p_glist_mem */

/*
 *  end of memory management
 */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************
 *
 *   plist_cat()
 *
 *************/

/* DOCUMENTATION
Concatenate two Plists and return the result.  The result is constructed
from the arguments, so do not refer to either of the arguments after the call.
<P>
That is, both args are destroyed.
*/

/* PUBLIC */
Plist plist_cat(Plist p1, Plist p2)
{
  if (p1 == NULL)
    return p2;
  else if (p2 == NULL)
    return p1;
  else {
    Plist p = p1;
    while (p->next != NULL)
      p = p->next;
    p->next = p2;
    return p1;
  }
}  /* plist_cat */

/*************
 *
 *   plist_cat2()
 *
 *************/

/* DOCUMENTATION
Concatenate two Plists and return the result.
In this version, the second plist is copied and
placed at the end of p1.
<P>
That is, the first arg is destroyed, and the second
is preserved.
*/

/* PUBLIC */
Plist plist_cat2(Plist p1, Plist p2)
{
  return plist_cat(p1, copy_plist(p2));
}  /* plist_cat2 */

/*************
 *
 *   plist_pop()
 *
 *************/

/* DOCUMENTATION
This routine takes a nonempty Plist, removes and frees the first node
(ignoring the contents), and returns the remainder of the list.
*/

/* PUBLIC */
Plist plist_pop(Plist p)
{
  Plist q = p;
  p = p->next;
  free_plist(q);
  return p;
}  /* plist_pop */

/*************
 *
 *   plist_count()
 *
 *************/

/* DOCUMENTATION
This routine returns the length of a Plist.
*/

/* PUBLIC */
int plist_count(Plist p)
{
  int n;
  for (n = 0; p; p = p->next, n++);
  return(n);
}  /* plist_count */

/*************
 *
 *   rev_app_plist(p1, p2)
 *
 *************/

static
Plist rev_app_plist(Plist p1, Plist p2)
{
  while (p1 != NULL) {
    Plist p3 = p1->next;
    p1->next = p2;
    p2 = p1;
    p1 = p3;
  }
  return p2;
}  /* rev_app_plist */

/*************
 *
 *     reverse_plist(p1)
 *
 *************/

/* DOCUMENTATION
This routine reverses a Plist.  The list is reversed in-place,
so you should not refer to the argument after calling this routine.
A good way to use it is like this:
<PRE>
p = reverse_plist(p);
</PRE>
*/

/* PUBLIC */
Plist reverse_plist(Plist p)
{
  return rev_app_plist(p, NULL);
}  /* reverse_plist */

/*************
 *
 *   copy_plist(p)
 *
 *************/

/* DOCUMENTATION
This routine copies a Plist (the whole list) and returns the copy.
*/

/* PUBLIC */
Plist copy_plist(Plist p)
{
  Plist start, p1, p2;

  start = NULL;
  p2 = NULL;
  for ( ; p; p = p->next) {
    p1 = get_plist();
    p1->v = p->v;
    p1->next = NULL;
    if (p2)
      p2->next = p1;
    else
      start = p1;
    p2 = p1;
  }
  return(start);
}  /* copy_plist */

/*************
 *
 *   zap_plist(p)
 *
 *************/

/* DOCUMENTATION
This routine frees a Plist (the whole list).  The things to which
the members point are not freed.
*/

/* PUBLIC */
void zap_plist(Plist p)
{
  Plist curr, prev;

  curr = p;
  while (curr != NULL) {
    prev = curr;
    curr = curr->next;
    free_plist(prev);
  }
}  /* zap_plist */

/*************
 *
 *   plist_append()
 *
 *************/

/* DOCUMENTATION
This routine appends a pointer to a Plist.  The updated Plist
is returned.
*/

/* PUBLIC */
Plist plist_append(Plist lst, void *v)
{
  Plist g = get_plist();
  Plist p;
  g->v = v;
  g->next = NULL;
  if (lst == NULL)
    return g;
  for (p = lst; p->next; p = p->next)
    ;
  p->next = g;
  return lst;
}  /* plist_append */

/*************
 *
 *   plist_prepend()
 *
 *************/

/* DOCUMENTATION
This routine inserts a pointer as the first member of a Plist.
The updated Plist is returned.
*/

/* PUBLIC */
Plist plist_prepend(Plist lst, void *v)
{
  Plist g = get_plist();
  g->v = v;
  g->next = lst;
  return g;
}  /* plist_prepend */

/*************
 *
 *   plist_member()
 *
 *************/

/* DOCUMENTATION
This function checks if a pointer is a member of a Plist.
*/

/* PUBLIC */
BOOL plist_member(Plist lst, void *v)
{
  Plist p;
  for (p = lst; p; p = p->next)
    if (p->v == v)
      return TRUE;
  return FALSE;
}  /* plist_member */

/*************
 *
 *   plist_subtract()
 *
 *************/

/* DOCUMENTATION
Return the members of p1 that are not in p2.
<P>
The arguments are not changed.
*/

/* PUBLIC */
Plist plist_subtract(Plist p1, Plist p2)
{
  /* The recursive version built the result from the tail back and prepended,
     which leaves it in p1's order; appending at a tail pointer does the same
     thing forwards.  New cells, p1 untouched, as before. */
  Plist head = NULL, tail = NULL, p;
  for (p = p1; p; p = p->next)
    if (!plist_member(p2, p->v)) {
      Plist g = get_plist();
      g->v = p->v;
      g->next = NULL;
      if (tail) tail->next = g; else head = g;
      tail = g;
    }
  return head;
}  /* plist_subtract */

/*************
 *
 *   plist_subset()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL plist_subset(Plist a, Plist b)
{
  Plist p;
  for (p = a; p; p = p->next)
    if (!plist_member(b, p->v))
      return FALSE;
  return TRUE;
}  /* plist_subset */

/*************
 *
 *   plist_remove()
 *
 *************/

/* DOCUMENTATION
Remove the first occurrence of a pointer from a Plist.
*/

/* PUBLIC */
Plist plist_remove(Plist p, void *v)
{
  Plist q, prev;
  for (q = p, prev = NULL; q; prev = q, q = q->next)
    if (q->v == v) {
      if (prev) prev->next = q->next; else p = q->next;
      free_plist(q);
      return p;
    }
  fatal_error("plist_remove: pointer not found");
  return p;  /* not reached */
}  /* plist_remove */

/*************
 *
 *   plist_remove_string()
 *
 *************/

/* DOCUMENTATION
Remove the first occurrence of a pointer from a Plist.
*/

/* PUBLIC */
Plist plist_remove_string(Plist p, char *s)
{
  Plist q, prev;
  for (q = p, prev = NULL; q; prev = q, q = q->next)
    if (str_ident(q->v, s)) {
      if (prev) prev->next = q->next; else p = q->next;
      free_plist(q);
      return p;
    }
  fatal_error("plist_remove_string: pointer not found");
  return p;  /* not reached */
}  /* plist_remove_string */

/*************
 *
 *   sort_plist()
 *
 *************/

/* DOCUMENTATION
 */

/* PUBLIC */
Plist sort_plist(Plist objects,	Ordertype (*comp_proc) (void *, void *))
{
  int n = plist_count(objects);
  void **a = malloc(n * sizeof(void *));
  int i;
  Plist p;
  for (p = objects, i = 0; p; p = p->next, i++)
    a[i] = p->v;
  merge_sort(a, n, comp_proc);
  for (p = objects, i = 0; p; p = p->next, i++)
    p->v = a[i];
  free(a);
  return objects;
}  /* sort_plist */

/*************
 *
 *   plist_remove_last()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Plist plist_remove_last(Plist p)
{
  Plist q, prev;
  if (p == NULL)
    return NULL;
  for (q = p, prev = NULL; q->next; prev = q, q = q->next)
    ;
  if (prev == NULL) {
    free_plist(q);
    return NULL;
  }
  prev->next = NULL;
  free_plist(q);
  return p;
}  /* plist_remove_last */

/*************
 *
 *   position_of_string_in_plist()
 *
 *************/

/* DOCUMENTATION
Count from 1; return -1 if the string is not in the Plist.
*/

/* PUBLIC */
int position_of_string_in_plist(char *s, Plist p)
{
  Plist q;
  int pos = 1;
  for (q = p; q; q = q->next, pos++)
    if (str_ident(s, q->v))
      return pos;
  return -1;
}  /* position_of_string_in_plist */

/*************
 *
 *   string_member_plist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL string_member_plist(char *s, Plist p)
{
  return position_of_string_in_plist(s, p) >= 0;
}  /* string_member_plist */

/*************
 *
 *   longest_string_in_plist()
 *
 *************/

/* DOCUMENTATION
Return -1 if the Plist is empty.
*/

/* PUBLIC */
int longest_string_in_plist(Plist p)
{
  Plist q;
  int n = -1;
  for (q = p; q; q = q->next)
    n = IMAX((int) strlen(q->v), n);
  return n;
}  /* longest_string_in_plist */

/*************
 *
 *   ith_in_plist()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void *ith_in_plist(Plist p, int i)
{
  Plist q;
  if (i <= 0)
    return NULL;
  for (q = p; q; q = q->next, i--)
    if (i == 1)
      return q->v;
  return NULL;
}  /* ith_in_plist */

/*************
 *
 *   plist_last()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void *plist_last(Plist p)
{
  Plist q;
  if (p == NULL)
    return NULL;
  for (q = p; q->next; q = q->next)
    ;
  return q->v;
}  /* plist_last */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************
 *
 *   ilist_cat()
 *
 *************/

/* DOCUMENTATION
Concatenate two Ilists and return the result.  The result is constructed
from the arguments, so do not refer to either of the arguments after the call.
<P>
That is, both arguments are "used up".
*/

/* PUBLIC */
Ilist ilist_cat(Ilist p1, Ilist p2)
{
  if (p1 == NULL)
    return p2;
  else if (p2 == NULL)
    return p1;
  else {
    Ilist p = p1;
    while (p->next != NULL)
      p = p->next;
    p->next = p2;
    return p1;
  }
}  /* ilist_cat */

/*************
 *
 *   ilist_cat2()
 *
 *************/

/* DOCUMENTATION
Concatenate two Ilists and return the result.
In this version, the second ilist is copied and
placed at the end of p1.  That is, p1 is "used up",
but p2 is not.
*/

/* PUBLIC */
Ilist ilist_cat2(Ilist p1, Ilist p2)
{
  Ilist p;
  if (p1 == NULL)
    return copy_ilist(p2);
  for (p = p1; p->next; p = p->next)
    ;
  p->next = copy_ilist(p2);
  return p1;
}  /* ilist_cat2 */

/*************
 *
 *   ilist_pop()
 *
 *************/

/* DOCUMENTATION
This routine takes a nonempty Ilist, removes and frees the first node
(ignoring the contents), and returns the remainder of the list.
*/

/* PUBLIC */
Ilist ilist_pop(Ilist p)
{
  Ilist q = p;
  p = p->next;
  free_ilist(q);
  return p;
}  /* ilist_pop */

/*************
 *
 *   ilist_count()
 *
 *************/

/* DOCUMENTATION
This routine returns the length of a Ilist.
*/

/* PUBLIC */
int ilist_count(Ilist p)
{
  int n;
  for (n = 0; p; p = p->next, n++);
  return(n);
}  /* ilist_count */

/*************
 *
 *   rev_app_ilist(p1, p2)
 *
 *************/

static
Ilist rev_app_ilist(Ilist p1, Ilist p2)
{
  while (p1 != NULL) {
    Ilist p3 = p1->next;
    p1->next = p2;
    p2 = p1;
    p1 = p3;
  }
  return p2;
}  /* rev_app_ilist */

/*************
 *
 *     reverse_ilist(p1)
 *
 *************/

/* DOCUMENTATION
This routine reverses a Ilist.  The list is reversed in-place,
so you should not refer to the argument after calling this routine.
A good way to use it is like this:
<PRE>
p = reverse_ilist(p);
</PRE>
*/

/* PUBLIC */
Ilist reverse_ilist(Ilist p)
{
  return rev_app_ilist(p, NULL);
}  /* reverse_ilist */

/*************
 *
 *   copy_ilist(p)
 *
 *************/

/* DOCUMENTATION
This routine copies a Ilist (the whole list) and returns the copy.
*/

/* PUBLIC */
Ilist copy_ilist(Ilist p)
{
  Ilist start, p1, p2;

  start = NULL;
  p2 = NULL;
  for ( ; p; p = p->next) {
    p1 = get_ilist();
    p1->i = p->i;
    p1->next = NULL;
    if (p2)
      p2->next = p1;
    else
      start = p1;
    p2 = p1;
  }
  return(start);
}  /* copy_ilist */

/*************
 *
 *   zap_ilist(p)
 *
 *************/

/* DOCUMENTATION
This routine frees a Ilist (the whole list).
*/

/* PUBLIC */
void zap_ilist(Ilist p)
{
  Ilist curr, prev;

  curr = p;
  while (curr != NULL) {
    prev = curr;
    curr = curr->next;
    free_ilist(prev);
  }
}  /* zap_ilist */

/*************
 *
 *   ilist_append()
 *
 *************/

/* DOCUMENTATION
This routine appends an integer to a Ilist.  The updated Ilist
is returned.
*/

/* PUBLIC */
Ilist ilist_append(Ilist lst, int i)
{
  Ilist g = get_ilist();
  Ilist p;
  g->i = i;
  g->next = NULL;
  if (lst == NULL)
    return g;
  for (p = lst; p->next; p = p->next)
    ;
  p->next = g;
  return lst;
}  /* ilist_append */

/*************
 *
 *   ilist_prepend()
 *
 *************/

/* DOCUMENTATION
This routine inserts an integer as the first member of a Ilist.
The updated Ilist is returned.
*/

/* PUBLIC */
Ilist ilist_prepend(Ilist lst, int i)
{
  Ilist g = get_ilist();
  g->i = i;
  g->next = lst;
  return g;
}  /* ilist_prepend */

/*************
 *
 *   ilist_last()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Ilist ilist_last(Ilist lst)
{
  Ilist p;
  if (lst == NULL)
    return NULL;
  for (p = lst; p->next; p = p->next)
    ;
  return p;
}  /* ilist_last */

/*************
 *
 *   ilist_member()
 *
 *************/

/* DOCUMENTATION
This function checks if an integer is a member of a Ilist.
(If a node in the Ilist contains a pointer instead of an integer,
the result is undefined.)
*/

/* PUBLIC */
BOOL ilist_member(Ilist lst, int i)
{
  Ilist p;
  for (p = lst; p; p = p->next)
    if (p->i == i)
      return TRUE;
  return FALSE;
}  /* ilist_member */

/*************
 *
 *   ilist_subtract()
 *
 *************/

/* DOCUMENTATION
Return the members of p1 that are not in p2.
<P>
The arguments are not changed.
*/

/* PUBLIC */
Ilist ilist_subtract(Ilist p1, Ilist p2)
{
  Ilist head = NULL, tail = NULL, p;
  for (p = p1; p; p = p->next)
    if (!ilist_member(p2, p->i)) {
      Ilist g = get_ilist();
      g->i = p->i;
      g->next = NULL;
      if (tail) tail->next = g; else head = g;
      tail = g;
    }
  return head;
}  /* ilist_subtract */

/*************
 *
 *   ilist_removeall()
 *
 *************/

/* DOCUMENTATION
Remove all occurrences of i.
<P>
The argument is "used up".
*/

/* PUBLIC */
Ilist ilist_removeall(Ilist p, int i)
{
  Ilist q, prev, next;
  for (q = p, prev = NULL; q; q = next) {
    next = q->next;
    if (q->i == i) {
      if (prev) prev->next = next; else p = next;
      free_ilist(q);
    }
    else
      prev = q;
  }
  return p;
}  /* ilist_removeall */

/*************
 *
 *   ilist_intersect()
 *
 *************/

/* DOCUMENTATION
Construct the intersection (as a new Ilist).
<P>
The arguments are not changed.
*/

/* PUBLIC */
Ilist ilist_intersect(Ilist a, Ilist b)
{
  if (a == NULL)
    return NULL;
  else {
    Ilist r = ilist_intersect(a->next, b);
    if (ilist_member(b, a->i))
      return ilist_prepend(r, a->i);
    else
      return r;
  }
}  /* ilist_intersect */

/*************
 *
 *   ilist_union()
 *
 *************/

/* DOCUMENTATION
Construct the union (as a new Ilist).
<p>
The arguments need not be sets, the result is a set.
<p>
The arguments are not changed.
*/

/* PUBLIC */
Ilist ilist_union(Ilist a, Ilist b)
{
  if (a == NULL)
    return ilist_set(b);  /* copies members of b (does not change b) */
  else if (ilist_member(b, a->i))
    return ilist_union(a->next, b);
  else
    return ilist_prepend(ilist_union(a->next, b), a->i);
}  /* ilist_union */

/*************
 *
 *   ilist_set()
 *
 *************/

/* DOCUMENTATION
Take a list of integers and remove duplicates.
This creates a new list and leave the old list as it was.
Don't make any assumptions about the order of the result.
*/

/* PUBLIC */
Ilist ilist_set(Ilist m)
{
  /* The recursive version worked from the tail back, dropping an element when
     its value was already in the set built from the suffix -- that is, when
     the value occurs again LATER.  So the equivalent forward test is "does
     this value appear later in the list"; the elements kept, and their order,
     are identical.  New cells; m is left alone, as before. */
  Ilist head = NULL, tail = NULL, p;
  for (p = m; p; p = p->next)
    if (!ilist_member(p->next, p->i)) {
      Ilist g = get_ilist();
      g->i = p->i;
      g->next = NULL;
      if (tail) tail->next = g; else head = g;
      tail = g;
    }
  return head;
}  /* ilist_set */

/*************
 *
 *   ilist_rem_dups()
 *
 *************/

/* DOCUMENTATION
Take a list of integers and remove duplicates.
<P>
This version "uses up" the argument.
*/

/* PUBLIC */
Ilist ilist_rem_dups(Ilist m)
{
  /* Same rule as ilist_set -- keep the last occurrence of each value -- but
     destructively, freeing the cells it drops.  See the comment there for why
     "already in the set built from the suffix" and "occurs again later" are
     the same test. */
  Ilist q, prev, next;
  for (q = m, prev = NULL; q; q = next) {
    next = q->next;
    if (ilist_member(next, q->i)) {
      if (prev) prev->next = next; else m = next;
      free_ilist(q);
    }
    else
      prev = q;
  }
  return m;
}  /* ilist_rem_dups */

/*************
 *
 *   ilist_is_set()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL ilist_is_set(Ilist a)
{
  Ilist p;
  for (p = a; p; p = p->next)
    if (ilist_member(p->next, p->i))
      return FALSE;
  return TRUE;
}  /* ilist_is_set */

/*************
 *
 *   ilist_subset()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL ilist_subset(Ilist a, Ilist b)
{
  Ilist p;
  for (p = a; p; p = p->next)
    if (!ilist_member(b, p->i))
      return FALSE;
  return TRUE;
}  /* ilist_subset */

/*************
 *
 *   fprint_ilist()
 *
 *************/

/* DOCUMENTATION
The list of integers is printed to FILE *fp like this: (4 5 1 3),
without a newline.
*/

/* PUBLIC */
void fprint_ilist(FILE *fp, Ilist p)
{
  fprintf(fp, "(");
  for (; p != NULL; p = p->next) {
    fprintf(fp, "%d", p->i);
    fprintf(fp, "%s", p->next ? " " : "");
  }
  fprintf(fp, ")");
  fflush(fp);
}  /* fprint_ilist */

/*************
 *
 *   p_ilist()
 *
 *************/

/* DOCUMENTATION
The list of integers is printed to stdout like this: (4 5 1 3),
with a '\n' at the end.
*/

/* PUBLIC */
void p_ilist(Ilist p)
{
  fprint_ilist(stdout, p);
  printf("\n");
  fflush(stdout);
}  /* p_ilist */

/*************
 *
 *   ilist_copy()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Ilist ilist_copy(Ilist p)
{
  Ilist head = NULL, tail = NULL, q;
  for (q = p; q; q = q->next) {
    Ilist new = get_ilist();
    new->i = q->i;
    new->next = NULL;
    if (tail) tail->next = new; else head = new;
    tail = new;
  }
  return head;
}  /* ilist_copy */

/*************
 *
 *   ilist_remove_last()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Ilist ilist_remove_last(Ilist p)
{
  Ilist q, prev;
  if (p == NULL)
    return NULL;
  for (q = p, prev = NULL; q->next; prev = q, q = q->next)
    ;
  if (prev == NULL) {
    free_ilist(q);
    return NULL;
  }
  prev->next = NULL;
  free_ilist(q);
  return p;
}  /* ilist_remove_last */

/*************
 *
 *   ilist_occurrences()
 *
 *************/

/* DOCUMENTATION
How many times does an integer occur in an ilist?
*/

/* PUBLIC */
int ilist_occurrences(Ilist p, int i)
{
  Ilist q;
  int n = 0;
  for (q = p; q; q = q->next)
    if (q->i == i)
      n++;
  return n;
}  /* ilist_occurrences */

/*************
 *
 *   ilist_insert_up()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
Ilist ilist_insert_up(Ilist p, int i)
{
  Ilist g, q, prev;
  for (q = p, prev = NULL; q && q->i < i; prev = q, q = q->next)
    ;
  g = get_ilist();
  g->i = i;
  g->next = q;
  if (prev == NULL)
    return g;
  prev->next = g;
  return p;
}  /* ilist_insert_up */

/*************
 *
 *   position_in_ilist()
 *
 *************/

/* DOCUMENTATION
Count from 1; return -1 if the int is not in the Ilist.
*/

/* PUBLIC */
int position_in_ilist(int i, Ilist p)
{
  Ilist q;
  int pos = 1;
  for (q = p; q; q = q->next, pos++)
    if (q->i == i)
      return pos;
  return -1;
}  /* position_in_ilist */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************
 *
 *   zap_i2list(p)
 *
 *************/

/* DOCUMENTATION
This routine frees an I2list (the whole list).
*/

/* PUBLIC */
void zap_i2list(I2list p)
{
  I2list curr, prev;

  curr = p;
  while (curr != NULL) {
    prev = curr;
    curr = curr->next;
    free_i2list(prev);
  }
}  /* zap_i2list */

/*************
 *
 *   i2list_append()
 *
 *************/

/* DOCUMENTATION
This routine appends an integer to a I2list.  The updated I2list
is returned.
*/

/* PUBLIC */
I2list i2list_append(I2list lst, int i, int j)
{
  I2list g = get_i2list();
  I2list p;
  g->i = i;
  g->j = j;
  g->next = NULL;
  if (lst == NULL)
    return g;
  for (p = lst; p->next; p = p->next)
    ;
  p->next = g;
  return lst;
}  /* i2list_append */

/*************
 *
 *   i2list_prepend()
 *
 *************/

/* DOCUMENTATION
This routine inserts an integer triple as the first member of a I2list.
The updated I2list is returned.
*/

/* PUBLIC */
I2list i2list_prepend(I2list lst, int i, int j)
{
  I2list g = get_i2list();
  g->i = i;
  g->j = j;
  g->next = lst;
  return g;
}  /* i2list_prepend */

/*************
 *
 *   i2list_removeall()
 *
 *************/

/* DOCUMENTATION
Remove all occurrences of i.
<P>
The argument is "used up".
*/

/* PUBLIC */
I2list i2list_removeall(I2list p, int i)
{
  I2list q, prev, next;
  for (q = p, prev = NULL; q; q = next) {
    next = q->next;
    if (q->i == i) {
      if (prev) prev->next = next; else p = next;
      free_i2list(q);
    }
    else
      prev = q;
  }
  return p;
}  /* i2list_removeall */

/*************
 *
 *   i2list_member()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
I2list i2list_member(I2list lst, int i)
{
  I2list p;
  for (p = lst; p; p = p->next)
    if (p->i == i)
      return p;
  return NULL;
}  /* i2list_member */

/*************
 *
 *   p_i2list()
 *
 *************/

/* DOCUMENTATION
The list of integers is printed to stdout like this: (4 5 1 3),
with a '\n' at the end.
*/

/* PUBLIC */
void p_i2list(I2list p)
{
  printf("(");
  for (; p != NULL; p = p->next) {
    printf("%d/%d", p->i, p->j);
    printf("%s", p->next ? " " : "");
  }
  printf(")\n");
  fflush(stdout);
}  /* p_i2list */

/*************
 *
 *   i2list_count()
 *
 *************/

/* DOCUMENTATION
This routine returns the length of a I2list.
*/

/* PUBLIC */
int i2list_count(I2list p)
{
  int n;
  for (n = 0; p; p = p->next, n++);
  return(n);
}  /* i2list_count */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************
 *
 *   i3list_member()
 *
 *************/

/* DOCUMENTATION
This function checks if a triple of integers is a member of a I3list.
*/

/* PUBLIC */
BOOL i3list_member(I3list lst, int i, int j, int k)
{
  I3list p;
  for (p = lst; p; p = p->next)
    if (p->i == i && p->j == j && p->k == k)
      return TRUE;
  return FALSE;
}  /* i3list_member */

/*************
 *
 *   i3list_append()
 *
 *************/

/* DOCUMENTATION
This routine appends an integer to a I3list.  The updated I3list
is returned.
*/

/* PUBLIC */
I3list i3list_append(I3list lst, int i, int j, int k)
{
  I3list g = get_i3list();
  I3list p;
  g->i = i;
  g->j = j;
  g->k = k;
  g->next = NULL;
  if (lst == NULL)
    return g;
  for (p = lst; p->next; p = p->next)
    ;
  p->next = g;
  return lst;
}  /* i3list_append */

/*************
 *
 *   i3list_prepend()
 *
 *************/

/* DOCUMENTATION
This routine inserts an integer triple as the first member of a I3list.
The updated I3list is returned.
*/

/* PUBLIC */
I3list i3list_prepend(I3list lst, int i, int j, int k)
{
  I3list g = get_i3list();
  g->i = i;
  g->j = j;
  g->k = k;
  g->next = lst;
  return g;
}  /* i3list_prepend */

/*************
 *
 *   zap_i3list(p)
 *
 *************/

/* DOCUMENTATION
This routine frees a I3list (the whole list).
*/

/* PUBLIC */
void zap_i3list(I3list p)
{
  I3list curr, prev;

  curr = p;
  while (curr != NULL) {
    prev = curr;
    curr = curr->next;
    free_i3list(prev);
  }
}  /* zap_i3list */

/*************
 *
 *   rev_app_i3list(p1, p2)
 *
 *************/

static
I3list rev_app_i3list(I3list p1, I3list p2)
{
  while (p1 != NULL) {
    I3list p3 = p1->next;
    p1->next = p2;
    p2 = p1;
    p1 = p3;
  }
  return p2;
}  /* rev_app_i3list */

/*************
 *
 *     reverse_i3list(p1)
 *
 *************/

/* DOCUMENTATION
This routine reverses a I3list.  The 3list is reversed in-place,
so you should not refer to the argument after calling this routine.
A good way to use it is like this:
<PRE>
p = reverse_i3list(p);
</PRE>
*/

/* PUBLIC */
I3list reverse_i3list(I3list p)
{
  return rev_app_i3list(p, NULL);
}  /* reverse_i3list */

/*************
 *
 *   copy_i3list(p)
 *
 *************/

/* DOCUMENTATION
This routine copies a I3list (the whole 3list) and returns the copy.
*/

/* PUBLIC */
I3list copy_i3list(I3list p)
{
  I3list start, p1, p2;

  start = NULL;
  p2 = NULL;
  for ( ; p; p = p->next) {
    p1 = get_i3list();
    p1->i = p->i;
    p1->j = p->j;
    p1->k = p->k;
    p1->next = NULL;
    if (p2)
      p2->next = p1;
    else
      start = p1;
    p2 = p1;
  }
  return(start);
}  /* copy_i3list */

/*************
 *
 *   i3list_count()
 *
 *************/

/* DOCUMENTATION
This routine returns the length of a I3list.
*/

/* PUBLIC */
int i3list_count(I3list p)
{
  int n;
  for (n = 0; p; p = p->next, n++);
  return(n);
}  /* i3list_count */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************
 *
 *   alist_insert()
 *
 *************/

/* DOCUMENTATION
Alists (association list) for integers.
Insert key/value pairs.  With assoc(key), retreive value.
If a key has more than one value, the most recent is returned.
It an key is not in the alist, INT_MIN is returned.
An alist can be freed with zap_i2list(alist).
This is not efficient, because no hashing is done; lookups are linear.
*/

/* PUBLIC */
I2list alist_insert(I2list p, int key, int val)
{
  return i2list_prepend(p, key, val);
}  /* alist_insert */

/*************
 *
 *   assoc()
 *
 *************/

/* DOCUMENTATION
See alist_insert.
*/

/* PUBLIC */
int assoc(I2list p, int key)
{
  I2list q;
  for (q = p; q; q = q->next)
    if (q->i == key)
      return q->j;
  return INT_MIN;
}  /* assoc */

/*************
 *
 *   alist2_insert()
 *
 *************/

/* DOCUMENTATION
Alist2 (association list) for pairs of integers.
Insert key/<value-a,value-b> pairs.
With assoc2a(key), retreive value-a.
With assoc2b(key), retreive value-b.
If a key has more than one value pair, the most recent is returned.
It a key is not in the alist2, INT_MIN is returned.
An alist2 can be freed with zap_i3list(alist2).
*/

/* PUBLIC */
I3list alist2_insert(I3list p, int key, int a, int b)
{
  return i3list_prepend(p, key, a, b);
}  /* alist2_insert */

/*************
 *
 *   assoc2a()
 *
 *************/

/* DOCUMENTATION
See alist2_insert.
*/

/* PUBLIC */
int assoc2a(I3list p, int key)
{
  I3list q;
  for (q = p; q; q = q->next)
    if (q->i == key)
      return q->j;
  return INT_MIN;
}  /* assoc2a */

/*************
 *
 *   assoc2b()
 *
 *************/

/* DOCUMENTATION
See alist2_insert.
*/

/* PUBLIC */
int assoc2b(I3list p, int key)
{
  I3list q;
  for (q = p; q; q = q->next)
    if (q->i == key)
      return q->k;
  return INT_MIN;
}  /* assoc2a */

/*************
 *
 *   alist2_remove()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
I3list alist2_remove(I3list p, int key)
{
  /* The recursive version fixed up the tail before testing the head, but the
     result is the same list with every key-matching cell gone, in order. */
  I3list q, prev, next;
  for (q = p, prev = NULL; q; q = next) {
    next = q->next;
    if (q->i == key) {
      if (prev) prev->next = next; else p = next;
      free_i3list(q);
    }
    else
      prev = q;
  }
  return p;
}  /* alist2_remove */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*
  Multiset of integers is implemented with I2list, that is,
  list of pairs of integers: <value, number-of-occurrences>.
 */

/*************
 *
 *   i2list_multimember()
 *
 *************/

/* DOCUMENTATION
Is <i,n> a multimember of multiset b?
*/

/* PUBLIC */
BOOL i2list_multimember(I2list b, int i, int n)
{
  I2list p;
  for (p = b; p; p = p->next)
    if (i == p->i)
      return n <= p->j;
  return FALSE;
}  /* i2list_multimember */

/*************
 *
 *   i2list_multisubset()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
BOOL i2list_multisubset(I2list a, I2list b)
{
  I2list p;
  for (p = a; p; p = p->next)
    if (!i2list_multimember(b, p->i, p->j))
      return FALSE;
  return TRUE;
}  /* i2list_multisubset */

/*************
 *
 *   multiset_add_n()
 *
 *************/

/* DOCUMENTATION
Add n occurrences of i to multiset a.
*/

/* This was recursive, one frame per distinct element already in the multiset,
   appending a new element at the tail:

     if (a == NULL)             { a = get_i2list(); a->i = i; a->j = n; }
     else if (a->i == i)          a->j += n;
     else                         a->next = multiset_add_n(a->next, i, n);

   Two problems, both reached by the same input -- the multiset of symbols in a
   large file, which has no cap on the number of distinct elements:

   1. The recursion is as deep as the multiset is long.  At 250,000 distinct
      symbols it overflows the default 8 MB stack and the process dies with no
      diagnostic.  That is what this rewrite fixes: it is the same walk, with
      the same tail append and therefore the same first-seen ordering, written
      as a loop.

   2. The walk itself is O(length) per insertion, so building a multiset of N
      distinct elements costs O(N^2).  Measured, distinct symbols against
      elapsed time to build:

          4,000 ->  105 ms     16,000 -> 1,576 ms
          8,000 ->  327 ms     32,000 -> 7,439 ms

      -- a 4x time for each 2x of input.  The same total number of occurrences
      spread over ONE symbol takes 45/87/236/794 ms, confirming the cost is the
      search and not the counting.  Raising the stack limit to 1 GB does not
      help the 250,000-symbol case: it stops segfaulting and instead runs past
      a 300-second CPU limit.  The crash was the symptom; this is the disease.

      Making the loop iterative cannot fix that -- a linear search is linear
      however it is written.  I2accum, below, is the fix, and the callers that
      can see an unbounded number of distinct symbols now use it.  This
      function is left as the small-multiset interface it always was (the
      multiset of variables in a clause, which MAX_VARS caps at 100), now
      without the stack-depth hazard. */

/* PUBLIC */
I2list multiset_add_n(I2list a, int i, int n)
{
  I2list p, prev;

  for (p = a, prev = NULL; p; prev = p, p = p->next)
    if (p->i == i) {
      p->j += n;
      return a;
    }

  /* not present: append at the tail, which is where the recursive version put
     it, so the multiset stays in first-seen order */
  p = get_i2list();
  p->i = i;
  p->j = n;
  if (prev == NULL)
    return p;
  prev->next = p;
  return a;
}  /* multiset_add_n */

/*************
 *
 *   multiset_add()
 *
 *************/

/* DOCUMENTATION
Add 1 occurrence of i to multiset a.
*/

/* PUBLIC */
I2list multiset_add(I2list a, int i)
{
  return multiset_add_n(a, i, 1);
}  /* multiset_add */

/*************
 *
 *   multiset_union()
 *
 *************/

/* DOCUMENTATION
The result is constructed from the arguments,
so do not refer to either of the arguments after the call.
That is, both arguments are "used up".
*/

/* PUBLIC */
I2list multiset_union(I2list a, I2list b)
{
  I2list p;
  for (p = b; p; p = p->next)
    a = multiset_add_n(a, p->i, p->j);
  zap_i2list(b);
  return a;
}  /* multiset_union */

/*************
 *
 *   multiset_to_set()
 *
 *************/

/* DOCUMENTATION
*/

/* This was recursive, one frame per element, which overflows the stack on the
   same large-multiset inputs that broke multiset_add_n above.  Iterative now,
   building the result front to back so the order is unchanged. */

/* PUBLIC */
Ilist multiset_to_set(I2list m)
{
  Ilist head = NULL, tail = NULL;
  I2list q;

  for (q = m; q; q = q->next) {
    Ilist p = get_ilist();
    p->i = q->i;
    p->next = NULL;
    if (tail == NULL)
      head = p;
    else
      tail->next = p;
    tail = p;
  }
  return head;
}  /* multiset_to_set */

/*************
 *
 *   multiset_occurrences()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
int multiset_occurrences(I2list m, int i)
{
  I2list a = i2list_member(m, i);
  return (a == NULL ? 0 : a->j);
}  /* multiset_occurrences */

/*************
 *
 *   Appending in O(1), with the caller holding the tail
 *
 *************/

/* WHY THESE EXIST
 *
 * A Plist IS the pointer to its first cell -- there is no list object, no
 * header holding first/last/count:
 *
 *     typedef struct plist * Plist;
 *     struct plist { void *v; Plist next; };
 *
 * So plist_append(lst, v) is handed only the head and has nowhere to keep a
 * tail pointer, even if it wanted one.  Walking to the end is forced by the
 * representation, not an oversight in the function -- and plist_prepend is
 * O(1) for the same reason, since prepending needs to know nothing about the
 * end.
 *
 * The consequence is a trap: the two look equally innocent at a call site,
 * and append is what you reach for whenever order matters, which is most of
 * the time.  "X = plist_append(X, v)" inside a loop over the input is
 * therefore quadratic, and the tree had 46 instances of exactly that shape.
 * Four of them turned up as measured bottlenecks in the course of the work in
 * FIXES.md -- the symbol multiset (PF2), the predicate-elimination candidate
 * set (PF3), the equational-definition dependents (PF7), and nonneg_clauses
 * (PF9), the worst of which cost 1.07 billion cell visits for an answer that
 * was then discarded.
 *
 * Giving the header struct to Plist would fix it everywhere, but Plist is
 * passed by value and returned across 92 call sites, Ilist across 58, and
 * I2list and I3list the same way; every signature in the library would
 * change.  So the tail lives with the caller instead, and these four
 * functions make that a one-liner rather than six lines of cell plumbing
 * repeated at each site:
 *
 *     Plist lst = NULL, tail = NULL;
 *     for (...)
 *       lst = plist_tail_append(lst, &tail, v);
 *
 * The result is identical to repeated plist_append: same cells, same order.
 * *tail must start NULL and belong to this list alone.
 */

/*************
 *
 *   plist_tail_append()
 *
 *************/

/* DOCUMENTATION
Append v to lst in O(1), using a tail pointer the caller holds.  Initialise
*tail to NULL alongside an empty lst, then pass the same tail on every call.
Equivalent to plist_append(lst, v) but without the walk.
*/

/* PUBLIC */
Plist plist_tail_append(Plist lst, Plist *tail, void *v)
{
  Plist g = get_plist();
  g->v = v;
  g->next = NULL;
  if (*tail)
    (*tail)->next = g;
  else
    lst = g;
  *tail = g;
  return lst;
}  /* plist_tail_append */

/*************
 *
 *   ilist_tail_append()
 *
 *************/

/* DOCUMENTATION
Append i to lst in O(1), using a tail pointer the caller holds.  See
plist_tail_append().
*/

/* PUBLIC */
Ilist ilist_tail_append(Ilist lst, Ilist *tail, int i)
{
  Ilist g = get_ilist();
  g->i = i;
  g->next = NULL;
  if (*tail)
    (*tail)->next = g;
  else
    lst = g;
  *tail = g;
  return lst;
}  /* ilist_tail_append */

/*************
 *
 *   i2list_tail_append()
 *
 *************/

/* DOCUMENTATION
Append (i,j) to lst in O(1), using a tail pointer the caller holds.  See
plist_tail_append().
*/

/* PUBLIC */
I2list i2list_tail_append(I2list lst, I2list *tail, int i, int j)
{
  I2list g = get_i2list();
  g->i = i;
  g->j = j;
  g->next = NULL;
  if (*tail)
    (*tail)->next = g;
  else
    lst = g;
  *tail = g;
  return lst;
}  /* i2list_tail_append */

/*************
 *
 *   i3list_tail_append()
 *
 *************/

/* DOCUMENTATION
Append (i,j,k) to lst in O(1), using a tail pointer the caller holds.  See
plist_tail_append().
*/

/* PUBLIC */
I3list i3list_tail_append(I3list lst, I3list *tail, int i, int j, int k)
{
  I3list g = get_i3list();
  g->i = i;
  g->j = j;
  g->k = k;
  g->next = NULL;
  if (*tail)
    (*tail)->next = g;
  else
    lst = g;
  *tail = g;
  return lst;
}  /* i3list_tail_append */

/* I2accum, the multiset-of-ints accumulator, used to live here as a
   hand-written open-addressed hash table.  It is implemented on
   std::unordered_map in i2accum_cxx.cpp now, behind the identical C
   interface declared in glist.h -- see the comment at the top of that file
   for why, and for the ordering guarantee the interface has to keep. */


