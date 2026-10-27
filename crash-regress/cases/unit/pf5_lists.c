/*
    Copyright (C) 2026 Chris Arena, 2026-10-27.
    Written in collaboration with Claude Opus 5.5 (Anthropic).

    NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
    not write this file.  It was written to re-run the reproducers that
    FIXES.md describes against other LADR builds, and carries the
    library's licence so that it can be distributed with it.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA. */

/* PF5 reproducer (FIXES.md "PF5 -- forty list functions recursed once per
   element"): the ten functions measured to SIGSEGV on a 400,000-element list
   with an 8 MB stack.  Each runs in its own forked child so that one crash
   cannot hide the others.  Lists are built with *_prepend, which is a loop in
   every version, so only the function under test can overflow the stack.
   Written for this regression run; uses only the public glist.h API. */
#include "../ladr/glist.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define N 400000
static Ilist big_ilist(void) { Ilist p = NULL; int i; for (i = 0; i < N; i++) p = ilist_prepend(p, i); return p; }
static Plist big_plist(void) { Plist p = NULL; int i; for (i = 0; i < N; i++) p = plist_prepend(p, "s"); return p; }
static Ilist sorted_ilist(void) { Ilist p = NULL; int i; for (i = N; i > 0; i--) p = ilist_prepend(p, i); return p; }
static I2list big_i2list(void) { I2list p = NULL; int i; for (i = 0; i < N; i++) p = i2list_prepend(p, i, 1); return p; }

static int failures = 0;
static void run(const char *name, int which)
{
  pid_t pid; int st = 0;
  fflush(stdout);
  pid = fork();
  if (pid == 0) {
    switch (which) {
    case 0: ilist_append(big_ilist(), -1); break;
    case 1: plist_append(big_plist(), "t"); break;
    case 2: ilist_copy(big_ilist()); break;
    case 3: ilist_occurrences(big_ilist(), 7); break;
    case 4: longest_string_in_plist(big_plist()); break;
    case 5: ilist_removeall(big_ilist(), 7); break;
    case 6: ilist_remove_last(big_ilist()); break;
    case 7: plist_remove_last(big_plist()); break;
    case 8: ilist_insert_up(sorted_ilist(), N + 1); break;
    case 9: multiset_to_set(big_i2list()); break;
    }
    _exit(0);
  }
  waitpid(pid, &st, 0);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
    printf("ok:   %-24s on %d elements\n", name, N);
  else {
    failures++;
    if (WIFSIGNALED(st))
      printf("FAIL: %-24s on %d elements: killed by signal %d (%s)\n", name, N,
             WTERMSIG(st), WTERMSIG(st) == SIGSEGV ? "SIGSEGV" : "other");
    else
      printf("FAIL: %-24s on %d elements: exit %d\n", name, N, WEXITSTATUS(st));
  }
}
int main(void)
{
  const char *names[] = {"ilist_append", "plist_append", "ilist_copy",
    "ilist_occurrences", "longest_string_in_plist", "ilist_removeall",
    "ilist_remove_last", "plist_remove_last", "ilist_insert_up", "multiset_to_set"};
  int i;
  for (i = 0; i < 10; i++) run(names[i], i);
  printf("pf5_lists: %d of 10 crashed\n", failures);
  return failures ? 1 : 0;
}
