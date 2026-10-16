/*  Copyright (C) 2026 Chris Arena, 2026-10-16.
    Written in collaboration with Claude Opus 5 (Anthropic).

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

/* Regression test for the discrimination index's key field.
 *
 * WHAT WAS WRONG
 *   struct discrim stored its key in a short:
 *
 *       struct discrim {
 *         Discrim next;
 *         union { Discrim kids; Plist data; } u;
 *         short symbol;     <-- here
 *         char  type;
 *       };
 *
 *   That field is overloaded.  What it holds depends on the sibling `type`:
 *
 *       type == DVARIABLE   a variable number, VARNUM(t)       <= MAX_VARS
 *       type == DRIGID      a SYMBOL number, SYMNUM(t)         unbounded
 *       AC indexing         an argument count, num_args        <= MAX_ARITY
 *
 *   Two of those three outgrow a short.  Symbol numbers are handed out by an
 *   incrementing counter with no cap -- SYM_TAB_SIZE is the width of the hash
 *   table, not a limit on symnums -- and MAX_ARITY is 65,534, which does not
 *   fit in a short at all.  Assigning an int symnum to a short is a silent
 *   truncation, and above 32,767 it goes NEGATIVE: symnum 40,000 reads back as
 *   -25,536.
 *
 *   The consequences are both in the index, and neither announces itself:
 *
 *     - insertion compares `d1->symbol != symbol` to decide whether a node for
 *       this symbol already exists.  A truncated field never equals the
 *       untruncated local, so every insertion of a high symbol creates a fresh
 *       sibling and the list stops being a set.
 *
 *     - retrieval compares the same way, so a term indexed under a high symbol
 *       is never found again.
 *
 *   The second one does not degrade quietly, which is what measurement showed
 *   and guesswork had not: the delete path reaches
 *
 *       fatal_error("discrim_bind_delete, cannot find end")
 *
 *   and the prover EXITS.  So this is not "indexing gets worse above 32,767
 *   symbols", it is "the prover aborts", and it needs a test that survives its
 *   own subject -- hence the fork below.
 *
 *   The field is int32_t now.  struct discrim is 24 bytes either way -- there
 *   were already five bytes of padding after the short -- so this costs
 *   nothing, which is the same free ride struct term got when arity widened.
 *
 * WHAT THIS TEST DOES
 *   Creates enough symbols to push greatest_symnum() past 32,767, builds a
 *   term over a symbol above the boundary, puts it in a discrimination index,
 *   and asks for it back.  Against the old field the retrieval fails; against
 *   int32_t it succeeds.  It also checks a symbol just below the boundary, so
 *   a failure points at the boundary rather than at the index in general.
 *
 *   It needs ~33,000 symbols, which takes a second or two, so it is tier2.
 */

#include "../ladr/discrimb.h"
#include "../ladr/discrim.h"
#include "../ladr/term.h"
#include "../ladr/symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int failures = 0;

static void check(const char *what, int ok)
{
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else       printf("ok:   %s\n", what);
}

/* A term f_i(a) for the i'th generated unary function symbol. */
static Term build(int sn_f, int sn_a)
{
  Term t = get_rigid_term_dangerously(sn_f, 1);
  ARG(t, 0) = get_rigid_term_dangerously(sn_a, 0);
  return t;
}

/* Index t, then ask for it back, IN A CHILD PROCESS.
 *
 * The fork is not defensive style, it is required: against the unfixed field
 * the delete path reaches fatal_error("discrim_bind_delete, cannot find end")
 * and exits, so a test that ran this inline would be killed by the very defect
 * it is reporting and would print nothing.  The child's exit status carries
 * the verdict instead.
 *
 *   0  the term went in and came back
 *   1  the term went in and did not come back
 *   2  the index aborted (fatal_error) or crashed
 */
static BOOL round_trip(Term t, void *datum)
{
  pid_t pid = -1;
  int status = 0;

  fflush(stdout);   /* or the child re-emits the parent's buffered output */
  pid = fork();

  if (pid < 0) {
    printf("FAIL: fork\n");
    return FALSE;
  }
  if (pid == 0) {
    Discrim root = discrim_init();
    Context subst = get_context();
    Discrim_pos pos = NULL;
    void *got = NULL;

    /* keep the child quiet; the exit status is the result */
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    discrim_bind_update(t, root, datum, INSERT);
    got = discrim_bind_retrieve_first(t, root, subst, &pos);
    if (got)
      discrim_bind_cancel(pos);
    free_context(subst);
    discrim_bind_update(t, root, datum, DELETE);
    destroy_discrim_tree(root);
    _exit(got == datum ? 0 : 1);
  }
  if (waitpid(pid, &status, 0) < 0)
    return FALSE;
  if (!WIFEXITED(status))
    return FALSE;                  /* killed by a signal */
  return WEXITSTATUS(status) == 0 ? TRUE : FALSE;
}

/* Why the term did not come back, for the report. */
static const char *round_trip_detail(Term t, void *datum)
{
  pid_t pid = -1;
  int status = 0;

  fflush(stdout);
  pid = fork();
  if (pid < 0) return "fork failed";
  if (pid == 0) {
    Discrim root = discrim_init();
    Context subst = get_context();
    Discrim_pos pos = NULL;
    void *got = NULL;
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    discrim_bind_update(t, root, datum, INSERT);
    got = discrim_bind_retrieve_first(t, root, subst, &pos);
    if (got) discrim_bind_cancel(pos);
    free_context(subst);
    _exit(got == datum ? 0 : 1);   /* stop before the delete */
  }
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status))
    return "the index crashed on insert or retrieve";
  if (WEXITSTATUS(status) == 1)
    return "retrieve did not find the term that was just inserted";
  return "insert and retrieve were fine; the delete path aborted";
}

int main(void)
{
  char name[64] = "";
  int i = 0, sn_a = 0, sn_low = -1, sn_high = 0;
  int target = 40000;          /* comfortably past 32,767 */

  sn_a = str_to_sn("a", 0);

  /* Generate symbols until the counter is past the short boundary.  The names
     are irrelevant; only the symnums matter. */
  for (i = 0; greatest_symnum() < target; i++) {
    snprintf(name, sizeof(name), "g%d", i);
    { int sn = str_to_sn(name, 1);
      if (sn_low < 0 && sn > 30000 && sn < 32000)
        sn_low = sn;                    /* one safely below the boundary */
    }
  }
  sn_high = str_to_sn("high_one", 1);

  printf("      greatest_symnum()=%d, below-boundary symbol=%d, above=%d\n",
	 greatest_symnum(), sn_low, sn_high);

  check("the test actually crossed the short boundary",
	sn_high > 32767 && sn_low > 0 && sn_low < 32767);

  /* Below the boundary: must work either way.  If this fails, the problem is
     not the field width and the next check proves nothing. */
  {
    Term t = build(sn_low, sn_a);
    check("a term under a symbol BELOW 32767 round-trips through the index",
	  round_trip(t, (void *) 0x1001) == TRUE);
    zap_term(t);
  }

  /* Above the boundary: this is the defect. */
  {
    Term t = build(sn_high, sn_a);
    BOOL ok = round_trip(t, (void *) 0x1002);
    if (!ok)
      printf("      detail: %s\n", round_trip_detail(t, (void *) 0x1002));
    check("a term under a symbol ABOVE 32767 round-trips through the index", ok == TRUE);
    zap_term(t);
  }

  /* And the field must hold MAX_ARITY, which the AC indexing stores in it. */
  check("the key field can hold MAX_ARITY",
	(int) MAX_ARITY <= 2147483647 && (int) MAX_ARITY > 32767);

  /* THE CHILD ORDER.  A node's rigid children must stay sorted by symbol, and
     its variable children must stay in front of them, however many children
     it has and whatever order they arrive in.
 
     This is the invariant that was not tested, and not testing it cost a
     capability regression.  The per-node child index (PF10) was built on the
     argument that nothing read the child order any more, so children could be
     appended in O(1) instead of spliced into sorted position.  Retrieval
     really does not walk the rigid list -- it looks the symbol up -- but the
     LIST order is the order of the depth-first walk over the tree, hence the
     order in which discrim_bind_retrieve_first() returns equally-matching
     answers, and demodulation and forward subsumption each take the FIRST
     answer.  Reordering the children changed which demodulator was applied.
     On Problems/SWV/SWV558-1.010.p a 0.03 s proof became 2 GB exhausted after
     79 s of search, and thirteen corpus problems went the same way.  The unit
     tests above all passed throughout: they check what the index RETURNS for
     a symbol, which was always right, and never what order the children are
     in.
 
     The symbols are inserted here in a scrambled order and the count crosses
     DI_PROMOTE_AT (32) several times over, so the indexed path is the one
     under test rather than the small-node fallback. */
  {
    Discrim root = discrim_init();
    int n = 400, k = 0;
    int *syms = malloc(n * sizeof(int));
    BOOL ordered = FALSE;

    for (k = 0; k < n; k++) {
      snprintf(name, sizeof(name), "ord%d", k);
      syms[k] = str_to_sn(name, 1);
    }
    /* A fixed scramble: stride 137 over 400 is coprime, so it visits every
       slot exactly once and lands nowhere near sorted order. */
    for (k = 0; k < n; k++) {
      Term t = build(syms[(k * 137) % n], sn_a);
      discrim_bind_update(t, root, (void *) (long) (k + 1), INSERT);
      zap_term(t);
    }

    ordered = discrim_children_ordered(root, 1);
    check("an indexed node's children stay in symbol order (400 scrambled inserts)",
	  ordered == TRUE);

    /* And after deletions, which is where the index keeps a separate record
       that could drift out of step with the list. */
    for (k = 0; k < n; k += 3) {
      Term t = build(syms[(k * 137) % n], sn_a);
      discrim_bind_update(t, root, (void *) (long) (k + 1), DELETE);
      zap_term(t);
    }
    check("...and after deleting every third one",
	  discrim_children_ordered(root, 1) == TRUE);

    free(syms);
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures != 0;
}
