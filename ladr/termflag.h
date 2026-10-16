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
    on 2026-10-13, and is not as William McCune released it in LADR-2009-11A.
    What changed and why is in FIXES.md; this file's own diff against the
    unmodified release is

        git diff 25b4505 -- ladr/termflag.h
*/

#ifndef TP_TERMFLAG_H
#define TP_TERMFLAG_H

#include "term.h"

/* INTRODUCTION
*/

/* Public definitions */

/* Is <flag> set in <bits>?  The mask is not 1, so "bits & flag" is a bit
   pattern rather than a truth value -- for the flag at bit 3 it is 8, not 1.
   Callers assign it to a BOOL, and a BOOL is expected to hold only TRUE or
   FALSE; nothing compares it against TRUE today, but a BOOL holding 8 is a
   trap waiting for the first caller that does. */

#define TP_BIT(bits, flag)     (((bits) & (flag)) ? TRUE : FALSE)

/* End of public definitions */

/* Public function prototypes from termflag.c */

int claim_term_flag();

void release_term_flag(int bit);

void term_flag_set(Term t, int flag);

void term_flag_clear(Term t, int flag);

BOOL term_flag(Term t, int flag);

int term_flags();

Term copy_term_with_flags(Term t);

Term copy_term_with_flag(Term t, int flag);

void term_flag_clear_recursively(Term t, int flag);

#endif  /* conditional compilation of whole file */
