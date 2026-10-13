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

/* Regression test for the width of the allocation counter.
 *
 * WHAT WAS WRONG
 *   ladr/memory.c counted bytes handed out by palloc() in
 *
 *       static unsigned Bytes_palloced;
 *
 *   and returned it through
 *
 *       int bytes_palloced(void);
 *
 *   so the count wrapped at 4GB and the accessor wrapped at 2GB.  Those two
 *   were the only things that actually tracked LADR's 500MB default ceiling.
 *   The ceiling itself is policy -- assign(max_megs,n) has always accepted up
 *   to INT_MAX, and -1 for no limit -- so anyone raising it past about 2000 on
 *   a modern machine got a silently wrong memory report and a wrong
 *   "Megabytes=" statistic, with no indication that the number had wrapped.
 *
 *   Both are now size_t.
 *
 * WHAT THIS TEST DOES
 *   Allocates past 2GB through tp_alloc(), the public path into palloc(), and
 *   checks that bytes_palloced() still reports a value that is (a) at least
 *   what was asked for, and (b) monotonically increasing.  Against the old
 *   code the accessor returns a wrapped, typically negative, value and the
 *   first check fails.
 *
 *   It needs a little over 2GB of memory and a couple of seconds.  That is
 *   the smallest honest test of a 2GB boundary: a smaller allocation would
 *   not reach the wrap, and asserting on anything less would not be testing
 *   the thing that was wrong.
 */

#include "../ladr/memory.h"

#define MEG            (1024 * 1024)
#define CHUNK_MEGS     4
#define TARGET_MEGS    2200          /* comfortably past the 2GB signed wrap */

int main(void)
{
  /* The reported value is held in a SIGNED type wide enough for both
     versions, deliberately.  Holding it in a size_t makes this test useless:
     the old accessor returns a negative int, which converts to a huge
     positive size_t, so every comparison below silently succeeds and the test
     passes against the very code it is meant to catch.  That is exactly what
     happened on the first attempt -- the old build printed "-1.85 GB" and the
     test still said ALL PASSED. */

  long long asked = 0;
  long long previous = 0;
  int i;
  int failures = 0;

  /* palloc() refuses to exceed max_megs, and this test deliberately goes
     past any sane default. */
  disable_max_megs();

  for (i = 0; i < TARGET_MEGS / CHUNK_MEGS; i++) {
    size_t n = (size_t) CHUNK_MEGS * MEG;
    long long reported;

    if (tp_alloc(n) == NULL) {
      printf("FAILURE: tp_alloc returned NULL at %.2f GB\n",
	     asked / (1024.0 * 1024.0 * 1024.0));
      failures++;
      break;
    }
    asked += (long long) n;
    reported = (long long) bytes_palloced();

    if (reported < previous) {
      printf("FAILURE: bytes_palloced() went backwards, %lld after %lld"
	     " (the counter wrapped)\n", reported, previous);
      failures++;
      break;
    }
    if (reported < asked) {
      printf("FAILURE: asked for %lld bytes, bytes_palloced() reports only"
	     " %lld\n", asked, reported);
      failures++;
      break;
    }
    previous = reported;
  }

  if (failures == 0) {
    if (asked <= 0x7fffffffLL) {
      printf("FAILURE: only reached %lld bytes, which does not cross the 2GB"
	     " boundary this test exists to check\n", asked);
      failures++;
    }
    else
      printf("memory_test: allocated %.2f GB, bytes_palloced() reports"
	     " %.2f GB, monotone throughout\n",
	     asked / (1024.0 * 1024.0 * 1024.0),
	     (long long) bytes_palloced() / (1024.0 * 1024.0 * 1024.0));
  }

  printf("memory_test: %s (%d failure(s))\n",
	 failures == 0 ? "ALL PASSED" : "FAILED", failures);
  return failures != 0;
}
