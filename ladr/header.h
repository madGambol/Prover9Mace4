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

/*  CHANGE NOTICE (GPL-2, section 2a): this file was modified by Chris Arena
    on 2026-10-11 through 2026-10-14, and is not as William McCune released it
    in LADR-2009-11A. What changed and why is in FIXES.md; this file's own
    diff against the unmodified release is

        git diff 25b4505 -- ladr/header.h
*/

#ifndef TP_HEADER_H
#define TP_HEADER_H

/* The most basic includes and definitions. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef enum { FALSE=0, TRUE=1 } BOOL;

/* ArityType: how many arguments a term or symbol takes.
 *
 * A named type because arity is a particular abstract quantity with its own
 * bound, not merely some integer that happens to be wide enough.  Before
 * this it was spelled "unsigned char" in struct term and struct flatterm and
 * "int" in sixty-odd declarations, and the two disagreed silently: anything
 * above 255 was truncated on its way into a term, so the term and its own
 * args array no longer agreed about how many arguments there were.
 *
 * Unsigned and narrow deliberately.  struct term is the most numerous object
 * in the system, and this width lets the field sit in padding that was
 * already there, so sizeof(struct term) is unchanged at 32 bytes.
 *
 * ARITY_NONE is the "this is not an arity" value, for a lookup that failed or
 * a symbol whose arity is unspecified.  Those cases used -1, which an
 * unsigned type cannot hold; giving the case a name is clearer than the bare
 * -1 was.  MAX_ARITY is one below it so the two can never collide.
 */

typedef uint16_t ArityType;

#define MAX_ARITY  ((ArityType) (UINT16_MAX - 1))  /* 65534 */
#define ARITY_NONE ((ArityType) UINT16_MAX)        /* not an arity */

/* The quantities around variables, each named for what it is.
 *
 * These were all spelled "int", which hid a genuine hazard: a term's
 * private_symbol field holds EITHER a variable number or a rigid symbol
 * number, told apart only by sign (VARNUM(t) is the field, SYMNUM(t) is its
 * negation).  Nothing distinguished the two quantities, so passing one where
 * the other belongs was silently wrong.
 *
 *   VarNumType    a variable's number, its identity.  0 .. MAX_VNUM.
 *   SymNumType    a rigid symbol's number.  1 .. MAX_SYM.
 *   VarCountType  how many distinct variables something has.  0 .. MAX_VARS.
 *
 * A caution about what this does and does not buy.  A C typedef is an alias,
 * not a distinct type: the compiler will not reject a SymNumType passed where
 * a VarNumType is wanted, because both are int.  What the names buy is that
 * the intent is stated at every declaration, and that VarCountType is
 * deliberately a different width, so mixing a count with a number is at least
 * visible to -Wconversion.  Detecting the rest needs the checked accessors in
 * term.h, which assert that a term really is the kind being asked about.
 */

typedef int32_t  VarNumType;
typedef int32_t  SymNumType;
typedef uint16_t VarCountType;

#define IMAX(a,b) ((a) > (b) ? (a) : (b))
#define IMIN(a,b) ((a) < (b) ? (a) : (b))

#define DBL_LARGE 10E11

/* Bytes needed to hold any int formatted with "%d", NUL included:
   "-2147483648" is 11 characters, so 12.  Use this when sizing a buffer
   that formats an integer, rather than guessing. */

#define INT_STRING_SIZE 12

/* PATH_MAX is in <limits.h> on POSIX systems, but it is permitted to be
   absent when the limit is indeterminate.  4096 is the Linux value and is a
   sane floor elsewhere. */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Bytes needed to hold a "file <name> not found"-style message, NUL included.
   Every such message names a file that was just handed to fopen(), and no
   path longer than PATH_MAX can be opened, so PATH_MAX bounds the longest
   name worth reproducing in full.  Add 32 for the longest fixed text any of
   those messages wraps around the name, then 20% margin.  All integer
   arithmetic, so this stays an integer constant expression and can size an
   array.  Use this when formatting a filename into a message, rather than
   guessing: the name comes from input or argv, so a guess gets truncated. */

#define FILE_MESSAGE_SIZE (((PATH_MAX + 32) * 6) / 5)

#endif  /* conditional compilation of whole file */
