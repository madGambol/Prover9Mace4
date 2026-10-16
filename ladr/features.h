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

        git diff 25b4505 -- ladr/features.h
*/

#ifndef TP_FEATURES_H
#define TP_FEATURES_H

#include "literals.h"

/* INTRODUCTION
*/

/* Public definitions */

/* End of public definitions */

/* Public function prototypes from features.c */

/* A feature vector is SPARSE: an I2list of (position, value) pairs in
   increasing position order, holding only the non-zero entries.  An absent
   position means zero.  See the long comment at the top of features.c for the
   layout and for why the index retrieves the same clauses as the dense
   version it replaces. */

void init_features(Ilist fsyms, Ilist rsyms);

I2list features(Literals lits);

int feature_length(void);

BOOL features_less_or_equal(I2list c, I2list d);

void p_features(I2list f);

#endif  /* conditional compilation of whole file */
