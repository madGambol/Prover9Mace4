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

/*
 *  intpower_test.c -- int_power() must saturate rather than wrap.
 *
 *  WHAT WAS WRONG
 *
 *  int_power(n, exp) is documented to return INT_MAX when the result does not
 *  fit in an int.  It detected that by multiplying and then checking whether
 *  the product had come out smaller than what it started from:
 *
 *      int x = r * n;
 *      if (x < r)
 *        return INT_MAX;    // overflow
 *      r = x;
 *
 *  Two things are wrong with that.  Signed integer overflow is undefined
 *  behaviour, so a compiler may assume it cannot happen and remove the test
 *  altogether.  And the test is incorrect even when it survives: it only
 *  fires if the wrapped value happens to land BELOW the previous one.
 *
 *  int_power(10, 10) is the case that shows it.  10^10 is 10,000,000,000,
 *  which modulo 2^32 is 1,410,065,408 -- larger than the 10^9 it was
 *  multiplied from, so "x < r" is false and the wrapped value is returned as
 *  if it were exact.  Measured at both -O0 and -O2 before the fix.
 *
 *  WHY IT MATTERS
 *
 *  int_power() is how interp.c sizes and bounds the tables of an
 *  interpretation -- six call sites, among them an array length and a loop
 *  limit.  A silently wrapped 1.4 billion is a plausible-looking number and
 *  the worst possible kind of wrong one.  compile_interp() did not even use
 *  this function: it had a bare "n = n * domain_size" loop with no check at
 *  all, feeding malloc(n * sizeof(int)) directly, and that is the code path
 *  interpformat and clausetester use -- the tools this project's soundness
 *  oracle relies on to re-verify a model independently.
 *
 *  THE FIX
 *
 *  Do the multiply in long long, which two ints can never overflow, and
 *  compare against INT_MAX/INT_MIN before narrowing.  Defined behaviour, and
 *  correct for every sign.
 *
 *  This test computes the expected value in double, which is exact for every
 *  case here, and asserts saturation exactly where the true result exceeds
 *  INT_MAX.  Against the unfixed build the 10^10 case fails.
 */

#include "../ladr/interp.h"
#include <stdio.h>
#include <limits.h>

static int failures = 0;

static void check(int n, int exp)
{
  int got = int_power(n, exp);
  double exact = 1.0;
  int i;
  int want;

  for (i = 0; i < exp; i++)
    exact *= (double) n;

  want = (exact > (double) INT_MAX) ? INT_MAX : (int) exact;

  if (got != want) {
    printf("FAIL: int_power(%d,%d) = %d, expected %d", n, exp, got, want);
    if (exact > (double) INT_MAX)
      printf("  (%.0f does not fit an int; it must saturate, not wrap)", exact);
    printf("\n");
    failures++;
  }
  else
    printf("ok:   int_power(%-2d,%-2d) = %-11d %s\n", n, exp, got,
           exact > (double) INT_MAX ? "(saturated)" : "");
}

int main(void)
{
  /* exact, and must stay exact */
  check(2, 0);
  check(2, 1);
  check(2, 10);
  check(2, 30);          /* 1073741824, the largest power of 2 that fits */
  check(10, 9);          /* 1000000000 */
  check(1, 1000);        /* no growth, must not be mistaken for overflow */
  check(0, 5);           /* 0, and must not divide by n anywhere */
  check(0, 0);           /* 1 by convention */

  /* must saturate */
  check(2, 31);
  check(2, 32);
  check(2, 40);
  check(3, 20);
  check(6, 12);
  check(10, 10);         /* the case the old check missed: wraps to 1410065408 */
  check(12, 9);

  /* a negative exponent is the documented -1, not an overflow */
  if (int_power(2, -1) != -1) {
    printf("FAIL: int_power(2,-1) = %d, expected -1\n", int_power(2, -1));
    failures++;
  }
  else
    printf("ok:   a negative exponent is -1\n");

  /* the arities Mace4 actually meets.  A clausified modal-logic problem
     produces Skolem functions of arity up to 29, and at domain size 2 that
     is 2^29 -- which fits -- while domain size 3 at the same arity does not.
     Both answers must be right, because one sizes a table and the other has
     to be refused. */
  if (int_power(2, 29) != 536870912) {
    printf("FAIL: int_power(2,29) = %d, expected 536870912\n", int_power(2, 29));
    failures++;
  }
  else
    printf("ok:   2^29 = 536870912 exactly (an arity Mace4 really meets)\n");

  if (int_power(3, 29) != INT_MAX) {
    printf("FAIL: int_power(3,29) = %d, expected INT_MAX\n", int_power(3, 29));
    failures++;
  }
  else
    printf("ok:   3^29 saturates (the same arity, one domain size larger)\n");

  if (failures == 0)
    printf("intpower_test: ALL PASSED (0 failure(s))\n");
  else
    printf("intpower_test: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
