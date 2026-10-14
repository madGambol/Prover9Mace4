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

#ifndef TP_CXX_ALLOC_H
#define TP_CXX_ALLOC_H

/* A C++ allocator that charges LADR's memory ceiling.
 *
 * WHY THIS HAS TO EXIST BEFORE ANY CONTAINER IS USED
 *   max_megs is enforced inside palloc(), by counting palloc's own 20 MB
 *   mallocs.  A std::unordered_map allocating through operator new is
 *   therefore invisible to it: assign(max_megs, n) would silently stop
 *   meaning what it says, and the two corpus problems that terminate BY
 *   exhausting the ceiling -- ALG033+1 and ALG043+1 -- would change behaviour
 *   for no reason a reader could find.
 *
 *   Every container in this tree is therefore declared with LadrAlloc<T>,
 *   which reports to charge_external_mem()/discharge_external_mem() so the
 *   ceiling counts those bytes alongside palloc's blocks.
 *
 * WHY NOT ALLOCATE FROM palloc ITSELF
 *   palloc is a bump allocator over 20 MB blocks and free_mem() pushes small
 *   blocks onto per-size avail lists; it never returns memory to the system
 *   and has no realloc.  A container that grows by doubling would strand every
 *   previous table in an avail list of a size nothing else uses.  Charging the
 *   budget while letting operator new do the work keeps the accounting honest
 *   without pretending the arena is a general allocator.
 *
 * EXCEPTIONS
 *   Nothing here throws except operator new, and that propagates to the
 *   extern "C" boundary, where the wrapper converts it -- see the try/catch in
 *   each entry point.  An exception must never cross into C.
 */

#include <cstddef>
#include <new>

extern "C" {
#include "memory.h"
}

template <typename T>
struct LadrAlloc {
  typedef T value_type;

  LadrAlloc() noexcept {}
  template <typename U> LadrAlloc(const LadrAlloc<U> &) noexcept {}

  T *allocate(std::size_t n)
  {
    const std::size_t bytes = n * sizeof(T);
    void *p = ::operator new(bytes);     /* throws std::bad_alloc on failure */
    charge_external_mem(bytes);
    return static_cast<T *>(p);
  }

  void deallocate(T *p, std::size_t n) noexcept
  {
    discharge_external_mem(n * sizeof(T));
    ::operator delete(static_cast<void *>(p));
  }

  template <typename U> struct rebind { typedef LadrAlloc<U> other; };
};

template <typename T, typename U>
bool operator==(const LadrAlloc<T> &, const LadrAlloc<U> &) noexcept
{ return true; }

template <typename T, typename U>
bool operator!=(const LadrAlloc<T> &, const LadrAlloc<U> &) noexcept
{ return false; }

#endif  /* conditional compilation of whole file */
