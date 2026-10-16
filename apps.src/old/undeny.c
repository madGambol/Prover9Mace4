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

#define PROGRAM_NAME    "undeny"
#include "../VERSION_DATE.h"

static char Help_string[] =
"\nThis program takes a sequence of clauses and for each negative ground\n"
"unit, it outputs the positive form, replacing constants with variables.\n"
"\n"
"Input is taken from stdin.\n"
"\n"
"Argument -c says to read commands and a list of clauses (mainly\n"
"so you can give 'op' commands); otherwise a simple stream of clauses\n"
"is read.\n\n";

/*************
 *
 *   constants_to_variables()
 *
 *************/

static
Term constants_to_variables(Term t)
{
  if (VARIABLE(t))
    return t;
  else if (CONSTANT(t)) {
    /* This is a bit of a shortcut.  Use the symnum of the constant
       as the variable number.  This will fail if there are too many
       consants (> 1000).  Assume variables will be renumbered later.
    */
    Term v = get_variable_term(SYMNUM(t));
    zap_term(t);
    return v;
  }
  else {
    int i;
    for (i = 0; i < ARITY(t); i++)
      ARG(t,i) = constants_to_variables(ARG(t,i));
    return t;
  }
}  /* constants_to_variables */

/*************
 *
 *   undeny_clause()
 *
 *************/

static
void undeny_clause(Clause c)
{
  /* Replace constants with variables, and negate each literal. */
  Literal lit;
  for (lit = c->literals; lit; lit = lit->next) {
    lit->atom = constants_to_variables(lit->atom);
    lit->sign = (lit->sign ? FALSE : TRUE);
  }
  renumber_variables(c, MAX_VARS);
}  /* undeny_clause */

int main(int argc, char **argv)
{
  Clause c;
  BOOL commands;
  int i;

  if (string_member("help", argv, argc) ||
      string_member("-help", argv, argc) ||
      string_member("-h", argv, argc)) {
    printf("\n%s, version %s, %s\n",PROGRAM_NAME,PROGRAM_VERSION,PROGRAM_DATE);
    printf("%s", Help_string);
    exit(1);
  }

  commands = string_member("-c", argv, argc);

  init_standard_ladr();
  i = register_attribute("label",  STRING_ATTRIBUTE);  /* ignore these */
  i = register_attribute("answer", TERM_ATTRIBUTE);  /* ignore these */

  skolem_check(FALSE);  /* don't check if Skolem symbols are already in use. */

  if (commands) {
    Term t = read_commands(stdin, stderr, FALSE, KILL_UNKNOWN);
    if (!(is_term(t, "clauses", 1) ||
	  is_term(t, "formulas", 1)))
      fatal_error("undeny: expecting clauses(...)");
  }
  
  /* Process each clause on stdin. */

  c = read_clause(stdin, stderr);

  while (c != NULL  && (!commands || !end_of_list_clause(c))) {

    if (unit_clause(c) && negative_clause(c))
      undeny_clause(c);

    fwrite_clause(stdout, c, CL_FORM_BARE);
    
    zap_clause(c);
    c = read_clause(stdin, stderr);
  }
  
  exit(0);
}  /* main */

