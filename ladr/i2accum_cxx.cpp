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

/* I2accum -- a multiset of ints -- on std::unordered_map, behind the same C
 * interface it had before.
 *
 * WHY
 *   I2accum was added (PF2 in ../FIXES.md) because building a multiset of
 *   symbols as a linked list is O(n^2): multiset_add_n() searched the list on
 *   every insertion.  The replacement was a hand-written open-addressed hash
 *   table -- about 150 lines of probing, tombstones, load factors and rehash.
 *
 *   By the end of that work there were THREE such tables in the tree (this
 *   one, the discrimination-tree child index, and the grouping inside
 *   collect_multiples) plus a sorted-array merge in arity_check.  Each is
 *   80-150 lines doing what unordered_map or sort does in one, and the cost is
 *   not speed, it is bug surface: the first version of the collect_multiples
 *   grouping introduced a FRESH quadratic, caught only by measuring it.  That
 *   is four hand-rolled containers and one self-inflicted defect.
 *
 *   So the container comes from the standard library and the C interface stays
 *   exactly as it was.  No caller changed; glist.h is untouched.
 *
 * WHAT THE INTERFACE PROMISES, AND WHY THE ORDER MATTERS
 *   i2accum_to_i2list() and i2accum_to_set() must return elements in
 *   FIRST-SEEN order.  That is not a convenience: symbol precedence is derived
 *   from these lists, precedence fixes the term ordering, and the term ordering
 *   decides which equations become rewrite rules -- so a different order here
 *   would silently change every proof.  unordered_map does not preserve
 *   insertion order, so the order is kept explicitly in a vector of keys, and
 *   the map holds the index into it.  test.src/multiset_test.c asserts the
 *   agreement element for element AND in order.
 *
 * ALLOCATION
 *   Both containers use LadrAlloc, so their bytes count against max_megs.
 *   See the comment in cxx_alloc.h for why that is not optional.
 *
 * EXCEPTIONS
 *   Every extern "C" entry point catches everything.  An exception crossing
 *   into C is undefined behaviour, so std::bad_alloc from a container is
 *   converted to LADR's own fatal_error, which is what a caller out of memory
 *   already expects.
 */

#include "cxx_alloc.h"

#include <unordered_map>
#include <vector>

extern "C" {
#include "glist.h"
}

namespace {

typedef std::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
			   LadrAlloc<std::pair<const int, int> > > IndexMap;
typedef std::vector<int, LadrAlloc<int> > IntVec;

}  /* anonymous namespace */

/* The accumulator.  `where` maps a key to its position in `key`/`count`,
   which are append-only and therefore in first-seen order. */

struct i2accum {
  IndexMap where;
  IntVec   key;
  IntVec   count;
};

/* One place to turn an escaped C++ exception into LADR's own failure, so that
   nothing throws across the extern "C" boundary. */
static void die(const char *what)
{
  fatal_error((char *) what);
}

extern "C" {

/* DOCUMENTATION
Return an empty multiset accumulator.  Free it with zap_i2accum(), or hand it
to i2accum_to_i2list(), which frees it for you.
*/
I2accum get_i2accum(void)
{
  try {
    return new i2accum();
  }
  catch (...) {
    die("get_i2accum, out of memory");
    return NULL;
  }
}

/* DOCUMENTATION
Free an accumulator.  Harmless on NULL.
*/
void zap_i2accum(I2accum acc)
{
  delete acc;        /* deleting NULL is well defined */
}

/* DOCUMENTATION
Add n occurrences of i to the accumulator.
*/
void i2accum_add_n(I2accum acc, int i, int n)
{
  try {
    IndexMap::iterator it = acc->where.find(i);
    if (it != acc->where.end()) {
      acc->count[it->second] += n;
      return;
    }
    acc->where[i] = (int) acc->key.size();
    acc->key.push_back(i);
    acc->count.push_back(n);
  }
  catch (...) {
    die("i2accum_add_n, out of memory");
  }
}

/* DOCUMENTATION
Add 1 occurrence of i to the accumulator.
*/
void i2accum_add(I2accum acc, int i)
{
  i2accum_add_n(acc, i, 1);
}

/* DOCUMENTATION
How many occurrences of i are in the accumulator?  0 if it is absent.
*/
int i2accum_occurrences(I2accum acc, int i)
{
  IndexMap::const_iterator it = acc->where.find(i);
  return (it == acc->where.end()) ? 0 : acc->count[it->second];
}

/* DOCUMENTATION
How many distinct elements are in the accumulator?
*/
int i2accum_distinct(I2accum acc)
{
  return (int) acc->key.size();
}

/* DOCUMENTATION
Convert to an ordinary I2list multiset, in first-seen order -- the same order
multiset_add_n() would have produced.  The accumulator is freed; do not refer
to it after the call.
*/
I2list i2accum_to_i2list(I2accum acc)
{
  I2list head = NULL, tail = NULL;
  std::size_t k = 0;

  if (acc == NULL)
    return NULL;

  for (k = 0; k < acc->key.size(); k++) {
    I2list p = get_i2list();
    p->i = acc->key[k];
    p->j = acc->count[k];
    p->next = NULL;
    if (tail == NULL)
      head = p;
    else
      tail->next = p;
    tail = p;
  }
  zap_i2accum(acc);
  return head;
}

/* DOCUMENTATION
The distinct elements of the accumulator, in first-seen order, as an Ilist.
The accumulator is NOT freed -- callers that want both the set and the counts
need it to survive.
*/
Ilist i2accum_to_set(I2accum acc)
{
  Ilist head = NULL, tail = NULL;
  std::size_t k = 0;

  if (acc == NULL)
    return NULL;

  for (k = 0; k < acc->key.size(); k++) {
    Ilist p = get_ilist();
    p->i = acc->key[k];
    p->next = NULL;
    if (tail == NULL)
      head = p;
    else
      tail->next = p;
    tail = p;
  }
  return head;
}

/* DOCUMENTATION
Build an accumulator holding the same multiset as the given I2list, which is
left alone.  A NULL list gives an empty accumulator.
*/
I2accum i2list_to_i2accum(I2list m)
{
  I2accum acc = get_i2accum();
  I2list p = NULL;
  for (p = m; p; p = p->next)
    i2accum_add_n(acc, p->i, p->j);
  return acc;
}

}  /* extern "C" */
