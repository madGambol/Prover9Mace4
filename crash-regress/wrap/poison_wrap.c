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

/* Link-time replica of the old project's -DPOISON_FREED_MEM (ladr/memory.c),
   applied WITHOUT editing any source: link with -Wl,--wrap=free_mem and this
   object.  Every call to free_mem() from another object file (free_flatterm,
   free_term, ...) is redirected here; we fill all but the first pointer of a
   pooled block (n < MAX_MEM_LISTS = 500, the avail-list path) with a
   non-canonical x86-64 address, then hand the block to the real free_mem(),
   which overwrites p[0] with the avail-list link exactly as before.  A later
   read through a stale pointer then faults instead of silently working. */
#define WRAP_MAX_MEM_LISTS 500
void __real_free_mem(void *q, unsigned n);
void __wrap_free_mem(void *q, unsigned n)
{
  if (q != 0 && n > 1 && n < WRAP_MAX_MEM_LISTS) {
    void **p = (void **) q;
    unsigned i;
    for (i = 1; i < n; i++)
      p[i] = (void *) 0xdeadbeefdeadbeefUL;
  }
  __real_free_mem(q, n);
}
