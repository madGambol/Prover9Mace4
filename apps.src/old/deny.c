/*  Copyright (C) 2006, 2007 William McCune

    NOTICE ADDED IN THIS TREE: the LADR-2009-11A release shipped this
    file without a licence notice.  What follows is the notice every
    .c and .h file of that release carries, reproduced so that this
    file is self-describing.  The terms are the release's own; this
    is not a new grant, and McCune did not write these lines here.

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

/*  CHANGE NOTICE (GPL-2, section 2a): this file was modified by
    Chris Arena on 2026-10-16, and is not as William McCune released it
    in LADR-2009-11A.  The only change is the licence notice above,
    which the release omitted from this file.
*/

#include "../ladr/top_input.h"
#include "../ladr/clausify.h"

#define PROGRAM_NAME    "deny"
#include "../VERSION_DATE.h"

static char Help_string[] =
"\nThis program takes a sequence of clauses and for each, it outputs\n"
"the denial.  This works for nonunit clauses, but it might not make\n"
"much sense for them.\n"
"\n"
"Input is taken from stdin.\n"
"\n"
"Argument -c says to read commands and a list of clauses (mainly\n"
"so you can give 'op' commands); otherwise a simple stream of clauses\n"
"is read.\n\n";

int main(int argc, char **argv)
{
  Formula f;
  BOOL commands;

  if (string_member("help", argv, argc) ||
      string_member("-help", argv, argc) ||
      string_member("-h", argv, argc)) {
    printf("\n%s, version %s, %s\n",PROGRAM_NAME,PROGRAM_VERSION,PROGRAM_DATE);
    printf("%s", Help_string);
    exit(1);
  }

  commands = string_member("-c", argv, argc);

  init_standard_ladr();
  skolem_check(FALSE);  /* don't check if Skolem symbols are already in use. */

  if (commands) {
    Term t = read_commands(stdin, stderr, FALSE, KILL_UNKNOWN);
    if (!(is_term(t, "clauses", 1) ||
	  is_term(t, "formulas", 1)))
      fatal_error("deny: expecting clauses(...) or formulas(...)");
  }
  
  /* Process each term on stdin. */

  f = read_formula(stdin, stderr);

  while (f != NULL  && (!commands || !end_of_list_formula(f))) {
    Plist clauses, p;

    f = universal_closure(f);
    f = negate(f);
    clauses = clausify_formula(f);
    for (p = clauses; p; p = p->next) {
      fwrite_clause(stdout, p->v, CL_FORM_BARE);
      zap_clause(p->v);
    }
    zap_plist(p);
    zap_formula(f);

    /* The following causes Skolem symbol numbering to restart
       at 1 for the next clause.
    */
    skolem_reset();

    f = read_formula(stdin, stderr);
  }
  
  exit(0);
}  /* main */

