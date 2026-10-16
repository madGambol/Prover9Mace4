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

/* Regression test for the multiset-of-ints packages in glist.c.
 *
 * WHAT WAS WRONG
 *   LADR keeps "the multiset of symbols in the input" as an I2list of
 *   (symbol, count) pairs.  multiset_add_n() searched that list linearly for
 *   each symbol added, and did so recursively:
 *
 *       if (a == NULL)        { a = get_i2list(); a->i = i; a->j = n; }
 *       else if (a->i == i)     a->j += n;
 *       else                    a->next = multiset_add_n(a->next, i, n);
 *
 *   so building a multiset of N distinct symbols cost O(N^2) comparisons and
 *   N stack frames.  At 250,000 distinct symbols -- a Cyc-sized knowledge base,
 *   not a hypothetical -- the recursion overflowed the 8 MB default stack and
 *   the process died with no diagnostic.  Raising the stack to 1 GB did not
 *   help: it stopped crashing and ran past a 300-second CPU limit instead.
 *   multiset_to_set() was recursive on the same list, for the same reason.
 *
 *   Separately, lex_order() in symbols.c called multiset_occurrences() -- a
 *   list walk -- once per symbol, which is the same quadratic on the reading
 *   side.  Fixing only the building half would have left startup quadratic.
 *
 * WHAT IS BEING TESTED
 *   1. Equivalence.  I2accum must produce exactly what multiset_add_n would
 *      have produced, in exactly the same order.  This is the check that
 *      matters most, and the reason is not obvious: the symbol multiset
 *      determines symbol precedence, precedence determines the term ordering,
 *      the term ordering determines which equations are used as rewrite rules.
 *      A multiset in a different order is not a cosmetic difference -- it
 *      silently changes every proof the prover finds.
 *
 *   2. Stack depth.  multiset_to_set() on a long multiset used to overflow;
 *      it is a loop now, so a 500,000-element multiset must survive.
 *
 *   3. Hash behaviour.  Deliberately colliding keys, negative keys and zero,
 *      because the slot array uses -1 for "empty" and the keys are arbitrary
 *      ints with no value free to use as a marker.
 *
 *   4. Scaling.  Building 200,000 distinct elements has to be fast.  The old
 *      code could not do it at all; the threshold here is loose enough not to
 *      be flaky on a loaded machine and far out of reach of a quadratic.
 *
 * It runs in well under a second, which is why it is in tier1.
 */

#include "../ladr/glist.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int failures = 0;

static void check(const char *what, int ok)
{
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else       printf("ok:   %s\n", what);
}

static int i2list_length(I2list p)
{
  int n = 0;
  for (; p; p = p->next) n++;
  return n;
}

int main(void)
{
  /* 1. Equivalence with the list implementation, including order.
   *
   * The sequence below is chosen to exercise every branch: a repeat of the
   * first element, a repeat of a middle element, a new element after repeats,
   * zero, and a negative key. */
  {
    static const int seq[] = { 7, 3, 7, 0, -5, 3, 7, 11, -5, 0, 0 };
    const int len = (int) (sizeof(seq) / sizeof(seq[0]));
    I2list ml = NULL;
    I2accum acc = get_i2accum();
    I2list ma, p, q;
    int i, same = 1;

    for (i = 0; i < len; i++) {
      ml = multiset_add(ml, seq[i]);
      i2accum_add(acc, seq[i]);
    }
    ma = i2accum_to_i2list(acc);     /* frees acc */

    check("same number of distinct elements",
	  i2list_length(ml) == i2list_length(ma));
    for (p = ml, q = ma; p && q; p = p->next, q = q->next)
      if (p->i != q->i || p->j != q->j) { same = 0; break; }
    check("I2accum agrees with multiset_add element for element, IN ORDER",
	  same && i2list_length(ml) == i2list_length(ma));

    /* The order is first-seen, which is what the recursive tail-append
       produced.  Spelling it out so a future change that reverses it fails
       here rather than in a proof three modules away. */
    check("first-seen order: 7 before 3 before 0 before -5 before 11",
	  ma && ma->i == 7 &&
	  ma->next && ma->next->i == 3 &&
	  ma->next->next && ma->next->next->i == 0 &&
	  ma->next->next->next && ma->next->next->next->i == -5 &&
	  ma->next->next->next->next && ma->next->next->next->next->i == 11);

    /* Counts: 7 appears 3 times, 3 twice, 0 three times, -5 twice, 11 once. */
    check("counts correct",
	  multiset_occurrences(ma, 7) == 3 &&
	  multiset_occurrences(ma, 3) == 2 &&
	  multiset_occurrences(ma, 0) == 3 &&
	  multiset_occurrences(ma, -5) == 2 &&
	  multiset_occurrences(ma, 11) == 1 &&
	  multiset_occurrences(ma, 999) == 0);

    zap_i2list(ml);
    zap_i2list(ma);
  }

  /* 2. multiset_add_n keeps appending at the tail now that it is a loop.
        Same check as above but driven through multiset_add_n with n > 1. */
  {
    I2list m = NULL;
    m = multiset_add_n(m, 5, 2);
    m = multiset_add_n(m, 9, 3);
    m = multiset_add_n(m, 5, 4);
    m = multiset_add_n(m, 1, 1);
    check("multiset_add_n appends at the tail",
	  m->i == 5 && m->next->i == 9 && m->next->next->i == 1);
    check("multiset_add_n accumulates n",
	  multiset_occurrences(m, 5) == 6 &&
	  multiset_occurrences(m, 9) == 3 &&
	  multiset_occurrences(m, 1) == 1);
    zap_i2list(m);
  }

  /* 3. Colliding keys.
   *
   * The table has a power-of-two slot count and probes linearly, so keys that
   * differ by a multiple of the capacity are the adversarial case.  Keys
   * spaced by 65536 collide in any table smaller than that. */
  {
    I2accum acc = get_i2accum();
    int i, wrong = 0;
    for (i = 0; i < 2000; i++)
      i2accum_add_n(acc, i * 65536, i + 1);
    for (i = 0; i < 2000; i++)
      if (i2accum_occurrences(acc, i * 65536) != i + 1) wrong++;
    check("2000 deliberately colliding keys all retrievable", wrong == 0);
    check("colliding keys counted as distinct", i2accum_distinct(acc) == 2000);
    check("absent key gives 0", i2accum_occurrences(acc, 12345) == 0);
    zap_i2accum(acc);
  }

  /* 4. Negative keys and zero, which the -1 "empty slot" marker must not
        confuse.  -1 is itself a legitimate key. */
  {
    I2accum acc = get_i2accum();
    i2accum_add(acc, -1);
    i2accum_add(acc, 0);
    i2accum_add(acc, -1);
    check("-1 is a key, not a marker",
	  i2accum_occurrences(acc, -1) == 2 &&
	  i2accum_occurrences(acc, 0) == 1 &&
	  i2accum_distinct(acc) == 2);
    zap_i2accum(acc);
  }

  /* 5. i2accum_to_set: distinct elements, first-seen order, accumulator left
        alive (both set and counts are needed by callers in formula.c). */
  {
    I2accum acc = get_i2accum();
    Ilist s;
    i2accum_add(acc, 4);
    i2accum_add(acc, 2);
    i2accum_add(acc, 4);
    s = i2accum_to_set(acc);
    check("i2accum_to_set gives distinct elements in first-seen order",
	  s && s->i == 4 && s->next && s->next->i == 2 && s->next->next == NULL);
    check("i2accum_to_set does not free the accumulator",
	  i2accum_occurrences(acc, 4) == 2);
    zap_ilist(s);
    zap_i2accum(acc);
  }

  /* 6. Stack depth: multiset_to_set() used to recurse once per element.
   *
   * 500,000 elements is about 24 MB of stack frames in the old code, well past
   * the 8 MB default, so this is a direct test of a fixed crash.  Building the
   * multiset through the accumulator keeps the setup linear -- doing it with
   * multiset_add_n would take longer than the rest of the suite put together,
   * which is itself the point of the change. */
  {
    const int n = 500000;
    I2accum acc = get_i2accum();
    I2list m;
    Ilist s;
    int i;
    for (i = 0; i < n; i++)
      i2accum_add(acc, i);
    m = i2accum_to_i2list(acc);
    check("500000-element multiset built", i2list_length(m) == n);
    s = multiset_to_set(m);          /* recursed once per element before */
    check("multiset_to_set on 500000 elements did not overflow the stack",
	  ilist_count(s) == n);
    check("multiset_to_set preserved order", s->i == 0);
    zap_ilist(s);
    zap_i2list(m);
  }

  /* 7. Scaling.  200,000 distinct keys, inserted and then read back.
   *
   * The old code needed about 2x10^10 comparisons for the insertions alone.
   * One second here is roughly a thousand times more headroom than the fixed
   * code needs, and utterly unreachable for a quadratic. */
  {
    const int n = 200000;
    I2accum acc = get_i2accum();
    clock_t t0 = clock();
    double build, read;
    int i, wrong = 0;

    for (i = 0; i < n; i++)
      i2accum_add(acc, i * 3 + 1);
    build = (double) (clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (i = 0; i < n; i++)
      if (i2accum_occurrences(acc, i * 3 + 1) != 1) wrong++;
    read = (double) (clock() - t0) / CLOCKS_PER_SEC;

    printf("      %d distinct keys: build %.3f s, read back %.3f s\n",
	   n, build, read);
    check("200000 distinct insertions under 1s (was quadratic)", build < 1.0);
    check("200000 lookups under 1s (was quadratic)", read < 1.0);
    check("200000 lookups all correct", wrong == 0);
    check("distinct count correct", i2accum_distinct(acc) == n);
    zap_i2accum(acc);
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures != 0;
}
