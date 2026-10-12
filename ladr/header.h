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
