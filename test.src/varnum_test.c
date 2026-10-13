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

/* Regression test for the named types around variables and symbols.
 *
 * WHAT THIS IS ABOUT
 *   A term's private_symbol field holds EITHER a variable number or a rigid
 *   symbol number, told apart only by sign:
 *
 *       VARIABLE(t)  is  private_symbol >= 0
 *       VARNUM(t)    is  private_symbol
 *       SYMNUM(t)    is -private_symbol
 *
 *   Every one of those quantities used to be spelled "int", so nothing
 *   distinguished a variable number from a symbol number from a count of
 *   variables.  Asking for the wrong one gives a plausible-looking number of
 *   the wrong kind -- SYMNUM() of a variable is a negative "symbol", VARNUM()
 *   of a constant is a negative "variable" -- which then gets used as an
 *   index.  There is three named types now:
 *
 *       VarNumType    a variable's number          0 .. MAX_VNUM
 *       SymNumType    a rigid symbol's number      1 .. MAX_SYM
 *       VarCountType  how many distinct variables  0 .. MAX_VARS
 *
 * WHAT A TYPEDEF DOES AND DOES NOT BUY
 *   A C typedef is an alias, not a distinct type, so the compiler will not
 *   reject a SymNumType passed where a VarNumType belongs: both are int.  The
 *   names state the intent at every declaration, and VarCountType is
 *   deliberately a different width so that mixing a count with a number is at
 *   least visible to -Wconversion.
 *
 *   Actual detection comes from building with -DCHECK_TERM_KIND, which turns
 *   SYMNUM() and VARNUM() into functions that assert the term really is the
 *   kind being asked about, and which also makes the compiler reject them
 *   being applied to a Flatterm.  That found real cross-type uses: the term
 *   macros were being applied to flatterms in three files -- 15 uses in
 *   flatterm.c, 38 in termorder.c and 3 in flatdemod.c -- because the macros
 *   were untyped and both structs happen to have a field of that name.  Those
 *   now use the FLAT_ accessors in flatterm.h.
 *
 * WHAT THIS TEST CHECKS
 *   That the types can hold the ranges their own limits claim, and that the
 *   two accessors agree with the term's kind.  It runs in microseconds, which
 *   is why it is in tier1.
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

int main(void)
{
  printf("varnum_test: MAX_VARS = %d, MAX_VNUM = %d,"
	 " sizeof(VarNumType) = %zu, sizeof(VarCountType) = %zu\n",
	 (int) MAX_VARS, (int) MAX_VNUM,
	 sizeof(VarNumType), sizeof(VarCountType));

  /* Each type must hold the range its own limit describes.  If MAX_VARS is
     ever raised past what VarCountType can hold, this fails rather than
     wrapping silently -- which is the whole point of naming the type. */
  {
    VarCountType n = (VarCountType) MAX_VARS;
    check(n == MAX_VARS,
	  "VarCountType cannot hold MAX_VARS; raising the limit has outgrown"
	  " the type");
    VarNumType v = (VarNumType) MAX_VNUM;
    check(v == MAX_VNUM, "VarNumType cannot hold MAX_VNUM");
  }

  /* A variable term: VARIABLE true, and VARNUM round-trips. */
  {
    Term v = get_variable_term(7);
    check(VARIABLE(v), "a variable term does not report VARIABLE");
    check(VARNUM(v) == 7, "a variable term did not round-trip its number");
    /* Variables are shared and never freed; see free_term(). */
  }

  /* A constant: VARIABLE false, and SYMNUM agrees with the symbol table. */
  {
    Term c = get_rigid_term("my_constant", 0);
    SymNumType sn = str_to_sn("my_constant", 0);
    check(!VARIABLE(c), "a constant reports VARIABLE");
    check(CONSTANT(c), "a constant does not report CONSTANT");
    check(SYMNUM(c) == sn,
	  "SYMNUM of a constant disagrees with the symbol table");
    free_term(c);
  }

  /* The two kinds must be mutually exclusive: that is the invariant the
     sign-encoded field relies on, and the one -DCHECK_TERM_KIND enforces. */
  {
    Term v = get_variable_term(3);
    Term c = get_rigid_term("another", 0);
    check(VARIABLE(v) && !CONSTANT(v), "a variable also looks like a constant");
    check(CONSTANT(c) && !VARIABLE(c), "a constant also looks like a variable");
    free_term(c);
  }

  printf("varnum_test: %s (%d failure(s))\n",
	 failures == 0 ? "ALL PASSED" : "FAILED", failures);
  return failures != 0;
}
