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
    on 2026-10-11 through 2026-10-15, and is not as William McCune released it
    in LADR-2009-11A. What changed and why is in FIXES.md; this file's own
    diff against the unmodified release is

        git diff 25b4505 -- ladr/memory.c
*/

#include "memory.h"

#define MALLOC_MEGS        20  /* size of blocks malloced by palloc */
/* The ceiling palloc() enforces, in megabytes, changeable with
   set_max_megs(n) or assign(max_megs,n).  500 was the 2009 figure.  Nothing
   in the design ties the limit to that number -- the option has always
   accepted anything up to INT_MAX, and -1 for no limit -- but two counters
   below did track it, and they are widened so that raising it is actually
   safe.

   WHAT THIS DEFAULT DOES AND DOES NOT GOVERN.  It applies only to programs
   that never call set_max_megs().  prover9 and mace4 both do call it, from
   their own max_megs PARAMETER, whose default is still 500:

       provers.src/search.c:  init_parm("max_megs", 500, -1, INT_MAX)
       mace4.src/msearch.c:   init_parm("max_megs", 500, -1, INT_MAX)
       provers.src/provers.c: set_max_megs(parm(prover_opt->max_megs))

   so raising the figure here did NOT raise the effective ceiling for the two
   provers.  Verified rather than assumed: prover9 on ALG021+1 still exits
   max_megs with its default, and does not with an explicit
   assign(max_megs, 1000).  Any earlier wording suggesting otherwise was
   wrong.

   What this default does govern is the auxiliary programs, and that is where
   it mattered: tptp_to_ladr parses only -q, never calls set_max_megs(), and
   so had no way at all to ask for more than the default.  Three HWV problems
   of 278-298 MB died at 500 and translate at 1000.

   Raising the two provers' parameter defaults as well is a separate decision,
   because it changes results: a search that stopped at 500 MB would run on,
   and memory-bounded runs would reach different conclusions.  Left at 500. */

#define DEFAULT_MAX_MEGS 1000
#define MAX_MEM_LISTS     500  /* number of lists of available nodes */

static void ** M[MAX_MEM_LISTS];

static BOOL Max_megs_check = TRUE;
static int Max_megs = DEFAULT_MAX_MEGS;  /* change with set_max_megs(n) */
static void (*Exit_proc) (void);         /* set with set_max_megs_proc() */

static int Malloc_calls = 0;   /* number of calls to malloc by palloc */

/* size_t, not unsigned.  As a 32-bit unsigned this wrapped at 4GB, and
   bytes_palloced() returned it as an int, which wrapped at 2GB -- so with
   max_megs raised past about 2000 the memory report and the Megabytes
   statistic silently went wrong.  These two were the only things that
   actually tracked the 500MB default. */

static size_t Bytes_palloced = 0;

/* Bytes allocated OUTSIDE palloc that must still count against max_megs.
 *
 * The ceiling is enforced below by counting palloc's own 20 MB mallocs, so
 * anything allocated by another allocator is invisible to it.  That matters
 * the moment a C++ container is used behind any of these interfaces: a
 * std::unordered_map allocating through operator new would not be counted,
 * assign(max_megs, n) would silently stop meaning what it says, and the two
 * corpus problems that terminate BY exhausting the ceiling -- ALG033+1 and
 * ALG043+1 -- would change behaviour for no stated reason.
 *
 * So external allocators charge here instead, through
 * charge_external_mem()/discharge_external_mem(), and the check below counts
 * those bytes alongside palloc's blocks.  See ladr/cxx_alloc.h. */

static size_t Bytes_external = 0;

/* Bytes_external in megabytes, maintained as it changes.  megs_malloced() is
   on Mace4's per-search-node path through mace_megs(), so it must not
   divide; charge/discharge happen far less often than the figure is read. */

static size_t Megs_external = 0;

/* These are char* rather than void* because palloc() does arithmetic on
   them.  Arithmetic on void* is a GCC extension (it treats void as having
   size 1), not standard C: a conforming compiler is entitled to reject
   "Block_pos += n".  char* gives the same byte arithmetic portably. */

static char *Block = NULL;        /* location returned by most recent malloc */
static char *Block_pos = NULL;    /* current position in block */

static unsigned Mem_calls = 0;
static unsigned Mem_calls_overflows = 0;

/* Poisoning blocks on their way to the avail lists.

   free_mem() does not hand a small block back to libc -- it links it onto
   M[n] -- so a use-after-free of a term, clause or literal is invisible both
   to valgrind and to ASan: from their point of view that memory is still
   live and validly owned by the process, and reusing it is quietly blessed.
   Only blocks of MAX_MEM_LISTS pointers or more reach free(), where a tool
   can see the mistake.

   Filling the block as it is released makes the mistake visible without any
   tool: POISON is non-canonical as an x86-64 address, so dereferencing a
   poisoned pointer faults immediately instead of happening to work, and the
   pattern is conspicuous in a debugger when read as an integer or a double.

   Off by default, because free_mem() is a hot path -- a search makes millions
   of these calls, which is why mega_mem_calls() exists to count them.  Build
   with -DPOISON_FREED_MEM to turn it on.  The first pointer of the block is
   deliberately left alone: it is the avail-list link. */

#ifdef POISON_FREED_MEM
#define POISON ((void *) 0xdeadbeefdeadbeefUL)
#endif

#define BUMP_MEM_CALLS {Mem_calls++; if (Mem_calls==0) Mem_calls_overflows++;}

/*************
 *
 *    void *palloc(n) -- assume n is a multiple of BYTES_POINTER.
 *
 *************/

static
void *palloc(size_t n)
{
  if (n == 0)
    return NULL;
  else {
    void *chunk;
    size_t malloc_bytes = MALLOC_MEGS*1024*1024;

    /* The cast keeps this a size_t comparison; the difference is the number
       of bytes left in the block, which cannot be negative here. */
    if (Block==NULL || (size_t) (Block + malloc_bytes - Block_pos) < n) {
      /* First call or not enough in the current block, so get a new block. */
      if (n > malloc_bytes) {
	printf("palloc, n=%d\n", (int) n);
	fatal_error("palloc, request too big; reset MALLOC_MEGS");
      }
      else if (Max_megs_check &&
	       (size_t) (Malloc_calls+1)*MALLOC_MEGS
		 + Bytes_external / (1024 * 1024) > (size_t) Max_megs) {
	if (Exit_proc)
	  (*Exit_proc)();
	else
	  fatal_error("palloc, Max_megs parameter exceeded");
      }
      else {
	Block_pos = Block = malloc(malloc_bytes);
	Malloc_calls++;
	if (Block_pos == NULL)
	  fatal_error("palloc, operating system is out of memory");
      } 
    }
    chunk = Block_pos; 
    Block_pos += n; 
    Bytes_palloced += n; 
    return(chunk);
  }
}  /* palloc */ 

/*************
 *
 *   get_cmem()
 *
 *************/

/* DOCUMENTATION
Get a chunk of memory that will hold n pointers (NOT n BYTES).
The memory is initialized to all 0.
*/

/* PUBLIC */
void *get_cmem(unsigned n)
{
  if (n == 0)
    return NULL;
  else {
    void **p = NULL;
    BUMP_MEM_CALLS;
    if (n >= MAX_MEM_LISTS) {
      /* See get_mem(): charged and checked for the same reasons. */
      p = calloc(n, BYTES_POINTER);
      if (p == NULL)
        fatal_error("get_cmem, out of memory");
      charge_external_mem((size_t) n * BYTES_POINTER);
      return p;
    }
    else if (M[n] == NULL)
      p = palloc(n * BYTES_POINTER);
    else {
      /* the first pointer is used for the avail list */
      p = M[n];
      M[n] = *p;
    }
    {
      int i;
      for (i = 0; i < n; i++)
	p[i] = 0;
    }
    return p;
  }
}  /* get_cmem */

/*************
 *
 *   get_mem()
 *
 *************/

/* DOCUMENTATION
Get a chunk of memory that will hold n pointers (NOT n BYTES).
The memory is NOT initialized.
*/


/* PUBLIC */
void *get_mem(unsigned n)
{
  if (n == 0)
    return NULL;
  else {
    void **p = NULL;
    BUMP_MEM_CALLS;
    if (n >= MAX_MEM_LISTS) {
      /* Blocks this big bypass palloc, so megs_malloced() cannot see them
         and max_megs could not bound them -- the same defect that let Mace4
         ask for 56 GB while reporting a 2 GB ceiling (M9).  Charge them to
         the external total, which megs_malloced() includes and
         charge_external_mem() enforces, and check the result rather than
         handing NULL back to a caller that will dereference it. */
      p = malloc(n * BYTES_POINTER);
      if (p == NULL)
        fatal_error("get_mem, out of memory");
      charge_external_mem((size_t) n * BYTES_POINTER);
    }
    else if (M[n] == NULL)
      p = palloc(n * BYTES_POINTER);
    else {
      /* the first pointer is used for the avail list */
      p = M[n];
      M[n] = *p;
    }
    return p;
  }
}  /* get_mem */

/*************
 *
 *   free_mem()
 *
 *************/

/* DOCUMENTATION
Free a chunk of memory that holds n pointers (not n bytes)
that was returned from a previous get_mem() or get_cmem() call.
*/

/* PUBLIC */
void free_mem(void *q, unsigned n)
{
  if (n == 0)
    ;  /* do nothing */
  else {
    /* put it on the appropriate avail list */
    void **p = q;
    if (n >= MAX_MEM_LISTS) {
      /* Charged by get_mem()/get_cmem(); keep the pair symmetric. */
      discharge_external_mem((size_t) n * BYTES_POINTER);
      free(p);
    }
    else {
#ifdef POISON_FREED_MEM
      /* Everything but the avail-list link; see POISON above. */
      unsigned i;
      for (i = 1; i < n; i++)
	p[i] = POISON;
#endif
      /* the first pointer is used for the avail list */
      *p = M[n];
      M[n] = p;
    }
  } 
}  /* free_mem */

/*************
 *
 *   mlist_length()
 *
 *************/

static
int mlist_length(void **p)
{
  int n;
  for (n = 0; p; p = *p, n++);
  return n;
}  /* mlist_length */

/*************
 *
 *   memory_report()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void memory_report(FILE *fp)
{
  int i;
  fprintf(fp, "\nMemory report, %d @ %d = %d megs (%.2f megs used).\n",
	  Malloc_calls, MALLOC_MEGS, Malloc_calls * MALLOC_MEGS,
	  Bytes_palloced / (1024 * 1024.0));
  for (i = 0; i < MAX_MEM_LISTS; i++) {
    int n = mlist_length(M[i]);
    if (n != 0)
      fprintf(fp, "List %3d, length %7d, %8.1f K\n", i, n,
	      i * n * BYTES_POINTER / 1024.);
  }
}  /* memory_report */

/*************
 *
 *    int megs_malloced() -- How many MB have been dynamically allocated?
 *
 *************/

/* DOCUMENTATION
This routine returns the number of megabytes that palloc()
has obtained from the operating system by malloc();
*/

/* PUBLIC */
int megs_malloced(void)
{
  /* Bytes_external holds what was allocated outside palloc but charged:
     the C++ containers behind I2accum and lookup_by_id, and blocks of
     MAX_MEM_LISTS pointers or more from get_mem()/get_cmem().  Leaving it
     out meant this figure, the "Total malloced" line built from it, and
     Mace4's own max_megs check were all blind to it. */
  return Malloc_calls * MALLOC_MEGS + (int) Megs_external;
}  /* megs_malloced */

/*************
 *
 *   charge_external_mem()
 *
 *************/

/* DOCUMENTATION
Charge n bytes allocated outside palloc against the max_megs ceiling, and
enforce the ceiling if this crosses it.  Used by allocators that are not
palloc -- see ladr/cxx_alloc.h -- so that assign(max_megs, n) keeps meaning
what it says.
*/

/* PUBLIC */
void charge_external_mem(size_t n)
{
  Bytes_external += n;
  Megs_external = Bytes_external / (1024 * 1024);
  if (Max_megs_check &&
      (size_t) Malloc_calls * MALLOC_MEGS
	+ Bytes_external / (1024 * 1024) > (size_t) Max_megs) {
    if (Exit_proc)
      (*Exit_proc)();
    else
      fatal_error("charge_external_mem, Max_megs parameter exceeded");
  }
}  /* charge_external_mem */

/*************
 *
 *   discharge_external_mem()
 *
 *************/

/* DOCUMENTATION
Release n bytes previously charged with charge_external_mem().
*/

/* PUBLIC */
void discharge_external_mem(size_t n)
{
  /* Saturate rather than wrap: a mismatched pair is a bug, but a wrapped
     size_t here would make the ceiling unreachable and hide it. */
  Bytes_external = (Bytes_external >= n) ? Bytes_external - n : 0;
  Megs_external = Bytes_external / (1024 * 1024);
}  /* discharge_external_mem */

/*************
 *
 *   external_mem_bytes()
 *
 *************/

/* DOCUMENTATION
How many bytes are currently charged from outside palloc.
*/

/* PUBLIC */
size_t external_mem_bytes(void)
{
  return Bytes_external;
}  /* external_mem_bytes */

/*************
 *
 *   set_max_megs()
 *
 *************/

/* DOCUMENTATION
This routine changes the limit on the amount of memory obtained
from malloc() by palloc().  The argument is in megabytes.
The default value is DEFAULT_MAX_MEGS.
*/

/* PUBLIC */
void set_max_megs(int megs)
{
  Max_megs = (megs == -1 ? INT_MAX : megs);
}  /* set_max_megs */

/*************
 *
 *   set_max_megs_proc()
 *
 *************/

/* DOCUMENTATION
This routine is used to specify the routine that will be called
if max_megs is exceeded.
*/

/* PUBLIC */
void set_max_megs_proc(void (*proc)(void))
{
  Exit_proc = proc;
}  /* set_max_megs_proc */

/*************
 *
 *   bytes_palloced()
 *
 *************/

/* DOCUMENTATION
How many bytes have been allocated by the palloc() routine?
This includes all of the get_mem() calls.
*/

/* PUBLIC */
size_t bytes_palloced(void)
{
  return Bytes_palloced;
}  /* bytes_palloced */

/*************
 *
 *   tp_alloc()
 *
 *************/

/* DOCUMENTATION
Allocate n bytes of memory, aligned on a pointer boundary.
The memory is not initialized, and it cannot be freed.
*/

/* PUBLIC */
void *tp_alloc(size_t n)
{
  /* If n is not a multiple of BYTES_POINTER, round up so that it is. */
  if (n % BYTES_POINTER != 0) {
    n += (BYTES_POINTER - (n % BYTES_POINTER));
  }
  return palloc(n);
}  /* tp_alloc */

/*************
 *
 *   mega_mem_calls()
 *
 *************/

/* DOCUMENTATION
 */

/* PUBLIC */
unsigned mega_mem_calls(void)
{
  return
    (Mem_calls / 1000000) +
    ((UINT_MAX / 1000000) * Mem_calls_overflows);
}  /* mega_mem_calls */

/*************
 *
 *   disable_max_megs()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void disable_max_megs(void)
{
  Max_megs_check = FALSE;
}  /* disable_max_megs */

/*************
 *
 *   enable_max_megs()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
void enable_max_megs(void)
{
  Max_megs_check = TRUE;
}  /* enable_max_megs */
