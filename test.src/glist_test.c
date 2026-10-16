/*  Copyright (C) 2026 Chris Arena

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

/* Regression test for the list functions in glist.c.
 *
 * WHAT WAS WRONG
 *   Forty of them recursed once per list element.  Not by accident -- it is
 *   the house style of the file, and for a list of ten it is the clearest way
 *   to write the function:
 *
 *       Plist plist_append(Plist lst, void *v)
 *       {
 *         if (lst == NULL) { ... return g; }
 *         else { lst->next = plist_append(lst->next, v); return lst; }
 *       }
 *
 *   The stack frame is about 48 bytes, so the 8 MB default limit is exhausted
 *   somewhere around 170,000 elements, and the process then dies with no
 *   diagnostic whatever -- no error message, no exit code that says what
 *   happened, just SIGSEGV.  These lists hold clauses and symbols, so their
 *   length is the size of the input: fine for a puzzle, fatal for a knowledge
 *   base.  The same defect in multiset_add_n (see PF2 in ../FIXES.md) was
 *   first seen as exactly that: an unexplained crash at 250,000 symbols.
 *
 *   NOT ALL FORTY ACTUALLY CRASHED, and the reason is worth recording.  Where
 *   the recursive call is in tail position -- ilist_member, assoc, plist_last,
 *   rev_app_ilist and most of the other scans -- gcc at -O turns it into a
 *   loop by itself, so those never overflowed in a release build.  The ones
 *   that crash are the ones that must do something after the call returns,
 *   which is most of the list-BUILDING and list-EDITING functions.  Measured
 *   on a 400,000-element list, 8 MB stack, both sides compiled -O:
 *
 *       ilist_append       SEGV -> ok      ilist_removeall    SEGV -> ok
 *       plist_append       SEGV -> ok      ilist_remove_last  SEGV -> ok
 *       ilist_copy         SEGV -> ok      plist_remove_last  SEGV -> ok
 *       ilist_occurrences  SEGV -> ok      ilist_insert_up    SEGV -> ok
 *       longest_string...  SEGV -> ok      multiset_to_set    SEGV -> ok
 *
 *   Ten of the twelve probed.  (position_in_ilist and ilist_cat2 survived;
 *   gcc evidently found an accumulator form for those two.)  Depending on the
 *   compiler to rescue the rest is not a plan -- it does not happen at -O0,
 *   which is what test.src itself builds with -- so all forty are loops now.
 *   No signature changed and no caller changed.
 *
 * WHAT IS BEING TESTED, AND WHY IN THIS SHAPE
 *   The risk in a change like this is not that a function stops working --
 *   that the test suite would catch at once. The risk is that one of them
 *   quietly returns the right elements in a different ORDER, or keeps a
 *   different one of two duplicates, because the recursion was doing
 *   something subtler than it looked.
 *
 *   Two of them were exactly that subtle. ilist_set and ilist_rem_dups built
 *   their result from the TAIL backwards and dropped an element when its value
 *   was already in the set built from the suffix -- which means they keep the
 *   LAST occurrence of each value, not the first:
 *
 *       ilist_set([3,1,4,1,5,9,2,6,5,3,5])  =  [4,1,9,2,6,3,5]
 *
 *   A forward loop written the obvious way keeps the first occurrence and
 *   returns [3,1,4,5,9,2,6] -- same length, same values, different list. The
 *   equivalent forward test is "does this value appear LATER", and that is
 *   what the code does now. alist2_remove was similar: it fixed up the tail
 *   before testing the head.
 *
 *   So this test is a transcript. EXPECTED below was produced by the
 *   RECURSIVE implementation, before the change, and is compared literally.
 *   Every value in it is therefore evidence rather than an assertion about
 *   what the functions ought to do -- including the orderings above, which no
 *   documentation in the file mentions.
 *
 *   Then four survival checks on lists of 400,000, which the recursive
 *   versions could not do at all.
 *
 * It runs in well under a second, which is why it is in tier1.
 */

#include "../ladr/glist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* The transcript, as the recursive implementation produced it.        */
/* ------------------------------------------------------------------ */

static const char EXPECTED[] =
"ilist_append: 3 1 4 1 5 9\n"
"plist_append: pear fig apple fig\n"
"i2list_append: (3,0) (1,1) (4,2) (1,3)\n"
"i3list_append: (3,0,0) (1,1,2) (4,2,4) (1,3,6)\n"
"reverse_ilist: 5 1 4 1 3\n"
"reverse_plist: fig apple fig pear\n"
"reverse_i3list: (1,2,3) (4,5,6)\n"
"reverse_ilist(empty):\n"
"ilist_member: 1 0\n"
"ilist_occurrences: 3 2 0\n"
"ilist_last: 5\n"
"position_in_ilist: 1 5 -1\n"
"ilist_is_set: 0\n"
"ilist_subset: 0 0\n"
"ilist_set: 4 1 9 2 6 3 5\n"
"ilist_copy: 3 1 4 1 5 9 2 6 5 3 5\n"
"ilist_is_set(set): 1\n"
"plist_member: 1 0\n"
"plist_last: cherry\n"
"ith_in_plist: pear apple 1 1\n"
"longest_string_in_plist: 6\n"
"position_of_string_in_plist: 2 5 -1\n"
"plist_subset: 1 0\n"
"longest_string_in_plist(empty): -1\n"
"ilist_last(empty): 1  plist_last(empty): 1\n"
"i2list_member: 5 1\n"
"i2list_multimember: 1 0 0\n"
"assoc: 5 1\n"
"i2list_multisubset: 1 0\n"
"i2list_removeall: (7,2) (9,1)\n"
"i3list_member: 1 0\n"
"assoc2a/b: 20 200 1\n"
"alist2_remove: (2,20,200)\n"
"ilist_subtract: 3 4 9 2 6 3\n"
"  a intact: 3 1 4 1 5 9 2 6 5 3 5\n"
"plist_subtract: pear apple cherry\n"
"  a intact: pear fig apple fig cherry\n"
"ilist_removeall: 3 1 4 1 9 2 6 3\n"
"ilist_rem_dups: 4 1 9 2 6 3 5\n"
"ilist_remove_last: 3 1 4\n"
"ilist_remove_last(one): 1\n"
"plist_remove_last: pear fig apple\n"
"plist_remove: pear fig fig cherry\n"
"plist_remove_string: pear apple fig cherry\n"
"ilist_cat2: 3 1 4 1 1 5 7\n"
"  b intact: 1 5 7\n"
"ilist_cat2(empty): 1 5 7\n"
"ilist_insert_up: 1 1 3 5 9\n"
"ilist_subtract(empty):\n"
"ilist_set(empty):\n"
"ilist_copy(empty):\n"
"ilist_removeall(empty):\n"
"ilist_is_set(empty): 1  ilist_subset(empty): 1\n"
"DONE\n";

/* ------------------------------------------------------------------ */

static char Got[8192];
static int  Gotn = 0;

static void out(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  Gotn += vsnprintf(Got + Gotn, sizeof(Got) - Gotn, fmt, ap);
  va_end(ap);
}

static void pi(const char *tag, Ilist p)
{ out("%s:", tag); for (; p; p = p->next) out(" %d", p->i); out("\n"); }
static void pp(const char *tag, Plist p)
{ out("%s:", tag); for (; p; p = p->next) out(" %s", (char *) p->v); out("\n"); }
static void p2(const char *tag, I2list p)
{ out("%s:", tag); for (; p; p = p->next) out(" (%d,%d)", p->i, p->j); out("\n"); }
static void p3(const char *tag, I3list p)
{ out("%s:", tag); for (; p; p = p->next) out(" (%d,%d,%d)", p->i, p->j, p->k); out("\n"); }

static Ilist mk(const int *a, int n)
{ Ilist r = NULL; int i; for (i = n - 1; i >= 0; i--) r = ilist_prepend(r, a[i]); return r; }
static Plist mks(char **a, int n)
{ Plist r = NULL; int i; for (i = n - 1; i >= 0; i--) r = plist_prepend(r, a[i]); return r; }

static int failures = 0;

static void check(const char *what, int ok)
{
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else       printf("ok:   %s\n", what);
}

int main(void)
{
  static const int A[] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5 };
  static const int B[] = { 1, 5, 7 };
  char *S[] = { "pear", "fig", "apple", "fig", "cherry" };
  char *T[] = { "fig", "plum" };
  int i;

  /* ---- append ---- */
  { Ilist p = NULL; for (i = 0; i < 6; i++) p = ilist_append(p, A[i]); pi("ilist_append", p); zap_ilist(p); }
  { Plist p = NULL; for (i = 0; i < 4; i++) p = plist_append(p, S[i]); pp("plist_append", p); zap_plist(p); }
  { I2list p = NULL; for (i = 0; i < 4; i++) p = i2list_append(p, A[i], i); p2("i2list_append", p); zap_i2list(p); }
  { I3list p = NULL; for (i = 0; i < 4; i++) p = i3list_append(p, A[i], i, i * 2); p3("i3list_append", p); zap_i3list(p); }

  /* ---- in-place reverse.  rev_app_* are internal to glist.c and declared
          nowhere; reverse_* is the public way in. ---- */
  { Ilist a = mk(A, 5); pi("reverse_ilist", reverse_ilist(a)); }
  { Plist a = mks(S, 4); pp("reverse_plist", reverse_plist(a)); }
  { I3list a = NULL; a = i3list_prepend(a, 1, 2, 3); a = i3list_prepend(a, 4, 5, 6);
    p3("reverse_i3list", reverse_i3list(a)); }
  pi("reverse_ilist(empty)", reverse_ilist(NULL));

  /* ---- membership, lookup, position, duplicate removal ---- */
  { Ilist a = mk(A, 11);
    out("ilist_member: %d %d\n", ilist_member(a, 9), ilist_member(a, 8));
    out("ilist_occurrences: %d %d %d\n", ilist_occurrences(a, 5), ilist_occurrences(a, 3), ilist_occurrences(a, 42));
    out("ilist_last: %d\n", ilist_last(a)->i);
    out("position_in_ilist: %d %d %d\n", position_in_ilist(3, a), position_in_ilist(5, a), position_in_ilist(42, a));
    out("ilist_is_set: %d\n", ilist_is_set(a));
    out("ilist_subset: %d %d\n", ilist_subset(mk(B, 3), a), ilist_subset(a, mk(B, 3)));
    pi("ilist_set", ilist_set(a));          /* keeps the LAST of each value */
    pi("ilist_copy", ilist_copy(a));
    zap_ilist(a); }
  { Ilist c = mk(B, 3); out("ilist_is_set(set): %d\n", ilist_is_set(c)); zap_ilist(c); }
  { Plist a = mks(S, 5);
    out("plist_member: %d %d\n", plist_member(a, S[2]), plist_member(a, T[1]));
    out("plist_last: %s\n", (char *) plist_last(a));
    out("ith_in_plist: %s %s %d %d\n", (char *) ith_in_plist(a, 1), (char *) ith_in_plist(a, 3),
	ith_in_plist(a, 0) == NULL, ith_in_plist(a, 99) == NULL);
    out("longest_string_in_plist: %d\n", longest_string_in_plist(a));
    out("position_of_string_in_plist: %d %d %d\n",
	position_of_string_in_plist("fig", a), position_of_string_in_plist("cherry", a),
	position_of_string_in_plist("kiwi", a));
    out("plist_subset: %d %d\n", plist_subset(mks(S, 2), a), plist_subset(mks(T, 2), a));
    zap_plist(a); }
  out("longest_string_in_plist(empty): %d\n", longest_string_in_plist(NULL));
  out("ilist_last(empty): %d  plist_last(empty): %d\n",
      ilist_last(NULL) == NULL, plist_last(NULL) == NULL);

  /* ---- i2list / i3list / alist ---- */
  { I2list a = NULL;
    a = i2list_append(a, 7, 2); a = i2list_append(a, 8, 5); a = i2list_append(a, 9, 1);
    out("i2list_member: %d %d\n", i2list_member(a, 8)->j, i2list_member(a, 99) == NULL);
    out("i2list_multimember: %d %d %d\n", i2list_multimember(a, 8, 5), i2list_multimember(a, 8, 6),
	i2list_multimember(a, 99, 1));
    out("assoc: %d %d\n", assoc(a, 8), assoc(a, 99) == INT_MIN);
    { I2list b = NULL; b = i2list_append(b, 8, 3);
      out("i2list_multisubset: %d %d\n", i2list_multisubset(b, a), i2list_multisubset(a, b)); }
    p2("i2list_removeall", i2list_removeall(a, 8)); }
  { I3list a = NULL;
    a = i3list_append(a, 1, 10, 100); a = i3list_append(a, 2, 20, 200); a = i3list_append(a, 1, 30, 300);
    out("i3list_member: %d %d\n", i3list_member(a, 2, 20, 200), i3list_member(a, 2, 20, 999));
    out("assoc2a/b: %d %d %d\n", assoc2a(a, 2), assoc2b(a, 2), assoc2a(a, 99) == INT_MIN);
    p3("alist2_remove", alist2_remove(a, 1)); }

  /* ---- subtract, remove, rem_dups, cat2, insert_up ---- */
  { Ilist a = mk(A, 11), b = mk(B, 3); pi("ilist_subtract", ilist_subtract(a, b)); pi("  a intact", a); }
  { Plist a = mks(S, 5), b = mks(T, 2); pp("plist_subtract", plist_subtract(a, b)); pp("  a intact", a); }
  { Ilist a = mk(A, 11); pi("ilist_removeall", ilist_removeall(a, 5)); }
  { Ilist a = mk(A, 11); pi("ilist_rem_dups", ilist_rem_dups(a)); }
  { Ilist a = mk(A, 4); pi("ilist_remove_last", ilist_remove_last(a)); }
  { Ilist a = mk(B, 1); out("ilist_remove_last(one): %d\n", ilist_remove_last(a) == NULL); }
  { Plist a = mks(S, 4); pp("plist_remove_last", plist_remove_last(a)); }
  { Plist a = mks(S, 5); pp("plist_remove", plist_remove(a, S[2])); }
  { Plist a = mks(S, 5); pp("plist_remove_string", plist_remove_string(a, "fig")); }
  { Ilist a = mk(A, 4), b = mk(B, 3); pi("ilist_cat2", ilist_cat2(a, b)); pi("  b intact", b); }
  { Ilist a = NULL, b = mk(B, 3); pi("ilist_cat2(empty)", ilist_cat2(a, b)); }
  { Ilist a = NULL; int U[] = { 5, 1, 9, 1, 3 };
    for (i = 0; i < 5; i++)
      a = ilist_insert_up(a, U[i]);
    pi("ilist_insert_up", a); zap_ilist(a); }

  /* ---- empty-list edges ---- */
  pi("ilist_subtract(empty)", ilist_subtract(NULL, mk(B, 3)));
  pi("ilist_set(empty)", ilist_set(NULL));
  pi("ilist_copy(empty)", ilist_copy(NULL));
  pi("ilist_removeall(empty)", ilist_removeall(NULL, 1));
  out("ilist_is_set(empty): %d  ilist_subset(empty): %d\n",
      ilist_is_set(NULL), ilist_subset(NULL, NULL));
  out("DONE\n");

  /* ---- the transcript must match the recursive implementation exactly ---- */
  if (strcmp(Got, EXPECTED) == 0)
    check("transcript matches the recursive implementation, byte for byte", 1);
  else {
    const char *g = Got, *e = EXPECTED;
    int line = 1;
    while (*g && *e && *g == *e) { if (*g == '\n') line++; g++; e++; }
    printf("FAIL: transcript differs, first difference on line %d\n", line);
    printf("  expected: %.70s\n", e);
    printf("  got:      %.70s\n", g);
    failures++;
  }

  /* ---- the O(1) tail-append helpers ----
   *
   * A Plist is just a pointer to its first cell, so plist_append() has
   * nowhere to keep a tail and must walk; "X = plist_append(X, v)" in a loop
   * over the input is therefore quadratic, and the tree had 46 instances of
   * that shape.  plist_tail_append() and friends let the caller hold the
   * tail.  The only thing that needs checking is that they build exactly what
   * the walking version builds -- same elements, same order -- because the
   * whole point is that call sites can be switched over without thinking.
   */
  {
    static const int V[] = { 9, 4, 4, 7, 0, -3, 12 };
    const int len = (int) (sizeof(V) / sizeof(V[0]));
    Ilist walked = NULL, tailed = NULL, tail = NULL;
    Plist pw = NULL, pt = NULL, ptail = NULL;
    I2list w2 = NULL, t2 = NULL, t2tail = NULL;
    I3list w3 = NULL, t3 = NULL, t3tail = NULL;
    int i, same = 1;

    for (i = 0; i < len; i++) {
      walked = ilist_append(walked, V[i]);
      tailed = ilist_tail_append(tailed, &tail, V[i]);
    }
    {
      Ilist a, b;
      for (a = walked, b = tailed; a && b; a = a->next, b = b->next)
	if (a->i != b->i) { same = 0; break; }
      check("ilist_tail_append matches ilist_append element for element",
	    same && ilist_count(walked) == len && ilist_count(tailed) == len);
    }
    check("ilist_tail_append leaves the tail at the last cell",
	  tail != NULL && tail->next == NULL && tail->i == V[len - 1]);

    same = 1;
    for (i = 0; i < len; i++) {
      pw = plist_append(pw, S[i % 5]);
      pt = plist_tail_append(pt, &ptail, S[i % 5]);
    }
    {
      Plist a, b;
      for (a = pw, b = pt; a && b; a = a->next, b = b->next)
	if (a->v != b->v) { same = 0; break; }
      check("plist_tail_append matches plist_append element for element",
	    same && plist_count(pw) == len && plist_count(pt) == len);
    }

    same = 1;
    for (i = 0; i < len; i++) {
      w2 = i2list_append(w2, V[i], i);
      t2 = i2list_tail_append(t2, &t2tail, V[i], i);
      w3 = i3list_append(w3, V[i], i, i * 2);
      t3 = i3list_tail_append(t3, &t3tail, V[i], i, i * 2);
    }
    {
      I2list a, b;
      I3list c, d;
      for (a = w2, b = t2; a && b; a = a->next, b = b->next)
	if (a->i != b->i || a->j != b->j) { same = 0; break; }
      for (c = w3, d = t3; c && d; c = c->next, d = d->next)
	if (c->i != d->i || c->j != d->j || c->k != d->k) { same = 0; break; }
      check("i2list_tail_append and i3list_tail_append match their walking forms",
	    same && i2list_count(w2) == len && i2list_count(t2) == len);
    }

    /* the empty case: an empty list with a NULL tail must come back as a
       one-element list, not be left empty */
    {
      Ilist e = NULL, etail = NULL;
      e = ilist_tail_append(e, &etail, 42);
      check("tail-append onto an empty list returns the new head",
	    e != NULL && e->i == 42 && etail == e && e->next == NULL);
      zap_ilist(e);
    }

    zap_ilist(walked); zap_ilist(tailed);
    zap_plist(pw); zap_plist(pt);
    zap_i2list(w2); zap_i2list(t2);
    zap_i3list(w3); zap_i3list(t3);
  }

  /* ---- and that appending 400,000 times is no longer quadratic ---- */
  {
    const int big = 400000;
    Ilist lst = NULL, tail = NULL;
    clock_t t0 = clock();
    double secs;
    int k;
    for (k = 0; k < big; k++)
      lst = ilist_tail_append(lst, &tail, k);
    secs = (double) (clock() - t0) / CLOCKS_PER_SEC;
    printf("      400000 ilist_tail_append took %.2f s\n", secs);
    check("400000 tail-appends under 1s (ilist_append would be quadratic)",
	  secs < 1.0);
    check("400000 tail-appends built the right list",
	  ilist_count(lst) == big && lst->i == 0 && tail->i == big - 1);
    zap_ilist(lst);
  }

  /* ---- stack depth: 400,000 elements, which the recursive versions could
          not reach.  Each of these walks the whole list. ---- */
  {
    const int n = 400000;
    Ilist big = NULL;
    Plist bigp = NULL;
    int k;

    for (k = 0; k < n; k++) big = ilist_prepend(big, k);
    for (k = 0; k < n; k++) bigp = plist_prepend(bigp, S[k % 5]);

    /* The eight below are the ones measured to SIGSEGV on the recursive
       version at -O; see the table in the header comment.  Each therefore
       fails against the unfixed library rather than merely passing here. */
    check("ilist_copy of 400000 elements (SEGV before)",
	  ilist_count(ilist_copy(big)) == n);
    check("ilist_occurrences over 400000 elements (SEGV before)",
	  ilist_occurrences(big, 7) == 1);
    check("longest_string_in_plist over 400000 elements (SEGV before)",
	  longest_string_in_plist(bigp) == 6);
    check("ilist_removeall over 400000 elements (SEGV before)",
	  ilist_count(ilist_removeall(ilist_copy(big), 7)) == n - 1);
    check("ilist_remove_last on 400000 elements (SEGV before)",
	  ilist_count(ilist_remove_last(ilist_copy(big))) == n - 1);
    check("plist_remove_last on 400000 elements (SEGV before)",
	  plist_count(plist_remove_last(copy_plist(bigp))) == n - 1);
    {
      Ilist sorted = NULL;
      for (k = n; k >= 1; k--)
	sorted = ilist_prepend(sorted, k);
      check("ilist_insert_up into a sorted 400000-element list (SEGV before)",
	    ilist_count(ilist_insert_up(sorted, n + 1)) == n + 1);
      zap_ilist(sorted);
    }

    /* And the scans, which gcc was already turning into loops at -O but not
       at -O0, which is how this file is built. */
    check("ilist_count on 400000 elements", ilist_count(big) == n);
    check("ilist_member walks 400000 elements", ilist_member(big, 0) == TRUE);
    check("ilist_last on 400000 elements", ilist_last(big)->i == 0);
    check("position_in_ilist on 400000 elements", position_in_ilist(0, big) == n);
    check("reverse_ilist on 400000 elements", (big = reverse_ilist(big)) != NULL && big->i == 0);
    check("plist_last on 400000 elements", plist_last(bigp) != NULL);
    check("reverse_plist on 400000 elements", (bigp = reverse_plist(bigp)) != NULL);

    /* ilist_append is O(n) per call by design, so appending n times is
       quadratic and not what this checks -- only that one append to a long
       list does not recurse 400,000 deep. */
    big = ilist_append(big, -1);
    check("ilist_append onto a 400000-element list", ilist_last(big)->i == -1);
    bigp = plist_append(bigp, S[0]);
    check("plist_append onto a 400000-element list", plist_last(bigp) == S[0]);

    zap_ilist(big);
    zap_plist(bigp);
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures != 0;
}
