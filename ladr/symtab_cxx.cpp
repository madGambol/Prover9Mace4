/*  Copyright (C) 2026 Chris Arena

    This file is part of the LADR Deduction Library.

    NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
    not write this file.  It was written for the audit of his release that
    FIXES.md describes, and carries the library's licence so that it can
    be distributed with it.

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

/* symnum -> Symbol, as a vector indexed by symnum.
 *
 * WHY THIS ONE FIRST
 *   lookup_by_id() is the most-called function in the library.  On
 *   Problems/CSR/CSR040-10.p it was 1,078,330,320 calls before the work in
 *   FIXES.md and is still 18,042,855 after it, and it was doing a hash plus a
 *   chain walk to answer a question that needs neither:
 *
 *       Symbol lookup_by_id(int symnum)
 *       {
 *         Plist p;
 *         for (p = By_id[hash_id(symnum)]; p; p = p->next) {
 *           Symbol s = p->v;
 *           if (s->symnum == symnum)
 *             return s;
 *         }
 *         return NULL;
 *       }
 *
 *   Symbol numbers are handed out by new_symnum(), which is
 *   "Symbol_count++; return Symbol_count;" -- so they are DENSE and
 *   SEQUENTIAL from 1.  A dense integer key wants an array, not a hash.
 *
 *   The hash made it worse than O(1) at scale as well: hash_id() is
 *   abs(id) % SYM_TAB_SIZE with SYM_TAB_SIZE == 50000, so from the
 *   50,001st symbol onward every bucket chains and the walk lengthens --
 *   exactly when a knowledge base needs it least.
 *
 * WHY A std::vector RATHER THAN A plain realloc'd array
 *   Because this is the file where the growth policy, the bounds check and
 *   the ownership live, and vector states all three in one line instead of
 *   three hand-written ones.  The same argument as i2accum_cxx.cpp: the win
 *   is not speed, it is the volume of container code not written.  This
 *   tree accumulated four hand-rolled containers during the FIXES.md work,
 *   and the first version of one of them contained a fresh quadratic.
 *
 * WHAT STAYS BEHIND
 *   By_sym, the name+arity hash, is untouched.  It is consulted while input
 *   is being read and does not appear in any profile.
 *
 * THE INTERFACE IS UNCHANGED
 *   symbols.c calls symtab_register() when it creates a symbol and
 *   lookup_by_id() everywhere else; symbols.h is not edited.
 */

#include "cxx_alloc.h"

#include <vector>

extern "C" {
#include "symbols.h"
}

namespace {

/* Index 0 is unused: symnums start at 1. */
std::vector<Symbol, LadrAlloc<Symbol> > *By_symnum = NULL;

}  /* anonymous namespace */

static void die(const char *what)
{
  fatal_error((char *) what);
}

extern "C" {

/* DOCUMENTATION
Record the symbol that owns this symnum, so lookup_by_id() can find it by
index.  Called by the symbol table when it creates a symbol.  Safe to call
with a symnum that already has an entry: the entry is replaced.
*/
void symtab_register(int symnum, Symbol s)
{
  try {
    if (By_symnum == NULL)
      By_symnum = new std::vector<Symbol, LadrAlloc<Symbol> >();
    if (symnum < 0)
      return;
    if ((std::size_t) symnum >= By_symnum->size())
      By_symnum->resize((std::size_t) symnum + 1, (Symbol) NULL);
    (*By_symnum)[(std::size_t) symnum] = s;
  }
  catch (...) {
    die("symtab_register, out of memory");
  }
}

/* DOCUMENTATION
Forget the symbol that owns this symnum.
*/
void symtab_unregister(int symnum)
{
  if (By_symnum == NULL || symnum < 0)
    return;
  if ((std::size_t) symnum < By_symnum->size())
    (*By_symnum)[(std::size_t) symnum] = (Symbol) NULL;
}

/* DOCUMENTATION
Return the Symbol with this symnum, or NULL.  An index, not a hash: symnums
are dense and sequential from 1.
*/
Symbol lookup_by_id(int symnum)
{
  if (By_symnum == NULL || symnum < 0)
    return NULL;
  if ((std::size_t) symnum >= By_symnum->size())
    return NULL;
  return (*By_symnum)[(std::size_t) symnum];
}

}  /* extern "C" */
