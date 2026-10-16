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

/* Regression test for the arity ceiling and the ArityType type.
 *
 * WHAT WAS WRONG
 *   MAX_ARITY was UCHAR_MAX, 255, with the comment "don't make this big", and
 *   the arity was stored in an "unsigned char" field in struct term and
 *   struct flatterm while sixty-odd declarations passed it around as "int".
 *   Two consequences:
 *
 *     - 255 is too small for real input.  TPTP v9.3.1 contains problems whose
 *       clausification produces Skolem functions of arity up to 280
 *       (esk1426_280, in the KRS and SYO families).  get_rigid_term() rejected
 *       them outright, so those problems could not be read at all.
 *
 *     - the narrow field and the wide parameters disagreed silently.  Nothing
 *       between the two checked the range, so an arity above the field's width
 *       was truncated on its way into the term: 280 became 280 - 256 = 24, and
 *       the term then disagreed with its own args array about how many
 *       arguments it had.  A wrong answer rather than an error.
 *
 * WHAT CHANGED
 *   A named type, ArityType, is now used everywhere the quantity is an arity,
 *   rather than whichever integer type happened to be at hand.  It is
 *   unsigned short, so MAX_ARITY is 65534 and ARITY_NONE (65535) names the
 *   "not an arity" case that used to be spelled -1.  The range check moved
 *   into get_term(), which is the single place a term is allocated, so no path
 *   can truncate.
 *
 * WHY THE WIDENING IS FREE
 *   struct term is the most numerous object in the system, so its size
 *   matters.  The field grew into padding that was already there:
 *
 *       int            private_symbol;   4 bytes
 *       ArityType      arity;            2
 *       FLAGS_TYPE     private_flags;    1
 *                                        1 byte of padding
 *       Term           *args;            8, and 8-aligned
 *
 *   This test asserts sizeof(struct term) is still 32, so that a later change
 *   which quietly makes terms bigger has to argue for itself.
 *
 * WHAT THIS TEST CHECKS
 *   That an arity above the old 255 ceiling round-trips exactly -- built,
 *   stored, and read back -- rather than being refused or truncated.  280 is
 *   the case TPTP actually needs; on the old code it is either a fatal error
 *   or 24.
 *
 * It runs in milliseconds, which is why it belongs in tier1.
 */

#include "../ladr/term.h"

static int failures = 0;

static void check(int condition, const char *what)
{
  if (!condition) {
    printf("FAILURE: %s\n", what);
    failures++;
  }
}

/* Build a rigid term of the given arity and read its arity back.
   Released with free_term(), not zap_term(): get_term() leaves the args array
   uninitialised, so recursing into it would be reading garbage pointers --
   which is exactly what the first version of this test did, and it segfaulted
   before testing anything. */

static void round_trip(int arity)
{
  Term t = get_rigid_term("f", arity);
  if (ARITY(t) != arity) {
    printf("FAILURE: built arity %d, term reports %d%s\n",
	   arity, (int) ARITY(t),
	   (int) ARITY(t) == arity % 256 ? " (truncated to 8 bits)" : "");
    failures++;
  }
  free_term(t);
}

int main(void)
{
  printf("arity_test: MAX_ARITY = %d, ARITY_NONE = %d,"
	 " sizeof(struct term) = %zu\n",
	 (int) MAX_ARITY, (int) ARITY_NONE, sizeof(struct term));

  /* The widening must not have cost memory: see the comment above. */
  check(sizeof(struct term) == 32,
	"sizeof(struct term) is no longer 32; the arity field has stopped"
	" fitting in padding and every term in the system just got bigger");

  /* The two sentinels must not collide, or "no arity" would be a legal one. */
  check(MAX_ARITY < ARITY_NONE, "MAX_ARITY collides with ARITY_NONE");

  /* The old ceiling, and the arity TPTP actually needs. */
  round_trip(0);
  round_trip(1);
  round_trip(2);
  round_trip(255);     /* the old maximum */
  round_trip(256);     /* one past it: truncated to 0 before this change */
  round_trip(280);     /* esk1426_280, from TPTP's KRS and SYO families */
  round_trip(1000);
  round_trip(MAX_ARITY);

  /* An arity must survive the symbol table too, not just the term. */
  {
    int sn = str_to_sn("g", 300);
    check(sn_to_arity(sn) == 300,
	  "the symbol table did not store an arity of 300 correctly");
  }

  /* ARITY_NONE must come back for a symbol that does not exist, which is the
     case that used to be reported as -1. */
  check(sn_to_arity(999999) == ARITY_NONE,
	"a missing symbol should give ARITY_NONE");

  printf("arity_test: %s (%d failure(s))\n",
	 failures == 0 ? "ALL PASSED" : "FAILED", failures);
  return failures != 0;
}
