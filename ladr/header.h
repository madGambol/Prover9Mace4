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

#ifndef TP_HEADER_H
#define TP_HEADER_H

/* The most basic includes and definitions. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

typedef enum { FALSE=0, TRUE=1 } BOOL;

#define IMAX(a,b) ((a) > (b) ? (a) : (b))
#define IMIN(a,b) ((a) < (b) ? (a) : (b))

#define DBL_LARGE 10E11

/* Bytes needed to hold any int formatted with "%d", NUL included:
   "-2147483648" is 11 characters, so 12.  Use this when sizing a buffer
   that formats an integer, rather than guessing. */

#define INT_STRING_SIZE 12

#endif  /* conditional compilation of whole file */
