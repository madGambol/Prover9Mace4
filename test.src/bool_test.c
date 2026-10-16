/*  Copyright (C) 2026 Chris Arena, 2026-10-16.
    Written in collaboration with Claude Opus 5 (Anthropic).

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

/* Regression test for the invariant that a BOOL holds only TRUE or FALSE.
 *
 * BOOL is an enum:
 *
 *     typedef enum { FALSE=0, TRUE=1 } BOOL;
 *
 * and C lets any int be assigned to it.  Compiling the library as C++ found 69
 * places doing exactly that.  Most were harmless -- a literal 0 or 1 passed as
 * an argument -- but the exercise turned up three real problems, and this test
 * pins the two that can be reached from outside their own file.
 *
 * WHY IT MATTERS AT ALL, GIVEN THAT C ALLOWS IT
 *   Two reasons.  Eight places in the library compare a BOOL against TRUE
 *   explicitly (for instance xproofs.c, "lit->sign == TRUE"), and such a
 *   comparison is wrong for any BOOL holding something other than 0 or 1.  And
 *   the width of an enum is the compiler's choice: GCC gives it int by default,
 *   but under -fshort-enums -- which is the default on ARM EABI -- a BOOL is
 *   one byte, so a value of 300 stored in one reads back as 44.
 *
 * THE THREE REAL FINDINGS
 *   1. provers.src/search.c declared
 *
 *          BOOL horn, unit, equality, number_of_clauses, number_of_neg_clauses;
 *
 *      -- five names on one line, the last two of a different kind.  They hold
 *      plist_count() sums and are compared against 100.  Under -fshort-enums a
 *      count of 300 becomes 44 and that comparison flips, selecting a different
 *      search strategy.  They are int now.  Not testable from here: the struct
 *      is static to search.c.
 *
 *   2. ladr/formula.c declared "BOOL dual_type(int op)" while returning Ftype
 *      enumerators up to EXISTS_FORM (8), and all three callers assign the
 *      result straight into a Formula's type field.  It returns Ftype now, and
 *      the check below confirms the mapping.
 *
 *   3. ladr/termflag.h defined
 *
 *          #define TP_BIT(bits, flag)  (bits & flag)
 *
 *      whose value is a bit pattern, not a truth value: for the flag at bit 3
 *      it is 8.  Callers assign it to a BOOL.  Nothing compares that particular
 *      BOOL against TRUE today, so it was a trap rather than a live bug -- but
 *      it is the exact shape of the two problems above.  It yields TRUE or
 *      FALSE now, which the check below confirms with a mask greater than 1.
 *
 * It runs in microseconds, which is why it is in tier1.
 */

#include "../ladr/termflag.h"
#include "../ladr/formula.h"

static int failures = 0;

static void check(int condition, const char *what)
{
  if (!condition) {
    printf("FAILURE: %s\n", what);
    failures++;
  }
}

int main(void)
{
  printf("bool_test: sizeof(BOOL) = %zu, FALSE = %d, TRUE = %d\n",
	 sizeof(BOOL), (int) FALSE, (int) TRUE);

  /* TP_BIT with a mask greater than 1.  Before the fix this returned the mask
     itself -- 8 for bit 3 -- and a BOOL then held 8. */
  {
    unsigned bits = 8u;          /* bit 3 set */
    BOOL b = TP_BIT(bits, 8u);
    check(b == TRUE,
	  "TP_BIT with a mask of 8 did not give TRUE; it is returning the bit"
	  " pattern rather than a truth value");
    check((int) b == 1,
	  "TP_BIT gave a BOOL holding something other than 0 or 1");
    b = TP_BIT(bits, 4u);        /* bit 2 clear */
    check(b == FALSE, "TP_BIT of a clear bit did not give FALSE");
  }

  /* A BOOL that holds only 0 or 1 is the thing that makes "== TRUE" safe.
     Eight places in the library rely on that. */
  {
    unsigned bits = 0xffu;
    BOOL b = TP_BIT(bits, 0x80u);
    check(b == TRUE, "a set high bit did not compare equal to TRUE");
  }

  /* dual_type returns a formula type, not a boolean. */
  check(dual_type(AND_FORM) == OR_FORM,     "dual_type(AND_FORM) != OR_FORM");
  check(dual_type(OR_FORM) == AND_FORM,     "dual_type(OR_FORM) != AND_FORM");
  check(dual_type(ALL_FORM) == EXISTS_FORM, "dual_type(ALL_FORM) != EXISTS_FORM");
  check(dual_type(EXISTS_FORM) == ALL_FORM, "dual_type(EXISTS_FORM) != ALL_FORM");
  check(dual_type(ATOM_FORM) == ATOM_FORM,  "dual_type should pass other types through");
  /* EXISTS_FORM is 8, which is why returning it as a BOOL was wrong. */
  check((int) EXISTS_FORM > 1,
	"EXISTS_FORM is no longer greater than 1, so this test has stopped"
	" checking what it was written to check");

  printf("bool_test: %s (%d failure(s))\n",
	 failures == 0 ? "ALL PASSED" : "FAILED", failures);
  return failures != 0;
}
