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

#include "../ladr/top_input.h"

#include <ctype.h>   /* isalnum, for the distinct-object prefix check */

#define PROGRAM_NAME    "tptp_to_ladr"
#include "../VERSION_DATE.h"

/*************
 *
 *   distinct objects
 *
 *************/

/* TPTP has a token LADR has no counterpart for: the "distinct object",
 * written in double quotes.
 *
 *     Documents/SyntaxBNF: <distinct_object>s are different from (but may be
 *     equal to) other tokens, e.g., "cat" is different from 'cat' and cat.
 *     Distinct objects are always interpreted as themselves, so if they are
 *     different they are unequal, e.g., "Apple" != "Microsoft" is implicit.
 *
 * LADR's reader quotes with ' here (set_quote_char below), so a double quote
 * is not even lexable and every problem containing one failed outright with
 * "sread_term error".  That is 239 of the 1,060 Commonsense Reasoning
 * problems, because Axioms/CSR003+{0,1,2}.ax render Cyc's numeric literals
 * this way ("-1", "0", "3.44"), plus the BIO and SYO families.  Mapping them
 * to LADR numerals is not an option: LADR cannot parse "3.44" at all, and
 * prover9 does not treat 1 and 2 as unequal without axioms saying so.
 *
 * So each distinct object becomes a fresh constant and their pairwise
 * inequality is stated explicitly, as n(n-1)/2 unit disequalities.
 *
 * A compact encoding was tried first and abandoned: give each object a
 * distinct bit pattern over ceil(log2 n) fresh predicates, which needs only
 * n*ceil(log2 n) formulas -- 6,170 rather than 190,036 for the 617 objects in
 * CSR003+2.ax.  It is a correct conservative extension, but it does not
 * survive prover9's own default preprocessing: with two objects the
 * disequality is provable, with five it is not, and it comes back only under
 * clear(auto).  A translator's output has to work with the default prover, so
 * the expensive encoding is the right one.  The cost is real and is reported
 * in the output.
 *
 * Nothing is said about whether a distinct object equals an ordinary term,
 * which is correct: TPTP permits "cat" = cat.
 */

#define DO_PREFIX "tptp_do_"

static Plist Distinct_objects = NULL;   /* texts, in order of first sighting */
static int   Distinct_count = 0;
static BOOL  Do_prefix_in_input = FALSE;  /* input already uses our prefix */

static
int distinct_object_index(char *text)
{
  Plist p;
  int i = 0;
  for (p = Distinct_objects; p != NULL; p = p->next, i++)
    if (str_ident(text, p->v))
      return i;
  Distinct_objects = plist_append(Distinct_objects, new_str_copy(text));
  return Distinct_count++;
}

/* Rewrite one TPTP stream, replacing each distinct object by its constant.
   Comments and single-quoted atoms are passed through untouched, except that
   a slash-star block comment is blanked: it is legal TPTP that LADR's reader
   does not know, and it was failing two SWV problems. */

static
FILE *rewrite_distinct_objects(FILE *fin)
{
  char *in = NULL, *out = NULL;
  size_t n = 0, cap = 0, i = 0, m = 0, outcap;
  FILE *mem;

  while (TRUE) {                        /* slurp */
    if (n + 65536 + 1 > cap) {
      cap = (cap == 0 ? 131072 : cap * 2);
      in = realloc(in, cap);
      if (in == NULL) fatal_error("rewrite_distinct_objects, out of memory");
    }
    {
      size_t got = fread(in + n, 1, 65536, fin);
      n += got;
      if (got < 65536) break;
    }
  }
  if (in == NULL) { in = malloc(1); if (in) in[0] = '\0'; }
  in[n] = '\0';

  outcap = n + 1024;
  out = malloc(outcap);
  if (out == NULL) fatal_error("rewrite_distinct_objects, out of memory");

#define PUT(c) do { if (m + 1 >= outcap) { outcap *= 2; out = realloc(out, outcap); \
                    if (out == NULL) fatal_error("rewrite_distinct_objects, out of memory"); } \
                    out[m++] = (c); } while (0)

  while (i < n) {
    char c = in[i];
    if (c == '%') {                             /* line comment */
      while (i < n && in[i] != '\n') PUT(in[i++]);
    }
    else if (c == '/' && i + 1 < n && in[i+1] == '*') {   /* block comment */
      i += 2;
      PUT(' '); PUT(' ');
      while (i < n && !(in[i] == '*' && i + 1 < n && in[i+1] == '/')) {
	PUT(in[i] == '\n' ? '\n' : ' ');
	i++;
      }
      if (i < n) { i += 2; PUT(' '); PUT(' '); }
    }
    else if (c == '\'') {                       /* single-quoted atom */
      PUT(in[i++]);
      while (i < n && in[i] != '\'') {
	if (in[i] == '\\' && i + 1 < n) PUT(in[i++]);
	PUT(in[i++]);
      }
      if (i < n) PUT(in[i++]);
    }
    else if (c == '"') {                        /* distinct object */
      char *text;
      size_t start, len = 0, k;
      char num[INT_STRING_SIZE + sizeof(DO_PREFIX)];
      i++;
      start = i;
      while (i < n && in[i] != '"') {
	if (in[i] == '\\' && i + 1 < n) { i++; len++; }
	i++; len++;
      }
      text = malloc(len + 1);
      if (text == NULL) fatal_error("rewrite_distinct_objects, out of memory");
      memcpy(text, in + start, len);
      text[len] = '\0';
      if (i < n) i++;                           /* closing quote */
      snprintf(num, sizeof(num), DO_PREFIX "%d", distinct_object_index(text));
      free(text);
      for (k = 0; num[k] != '\0'; k++) PUT(num[k]);
    }
    else {
      /* Watch for the generated prefix appearing as a real token.  This has
	 to be checked here rather than over the whole buffer, because the
	 buffer includes comments, and a comment that merely mentions the
	 prefix is not a collision -- TPTP problems are heavily commented. */
      if ((c == 't' || c == 'T') &&
	  strncmp(in + i, DO_PREFIX, strlen(DO_PREFIX)) == 0 &&
	  (i == 0 || (!isalnum((unsigned char) in[i-1]) && in[i-1] != '_')))
	Do_prefix_in_input = TRUE;
      PUT(in[i++]);
    }
  }
#undef PUT
  /* No terminating NUL: fmemopen is given an explicit length, and a NUL in
     the stream is read as a token and rejected by the parser. */

  free(in);
  mem = fmemopen(out, m, "r");            /* out is intentionally not freed:
                                             fmemopen reads from it */
  if (mem == NULL)
    fatal_error("rewrite_distinct_objects, fmemopen failed");
  return mem;
}

/* The pairwise disequalities, as TPTP text, so the ordinary reader takes
   care of them. */

static
FILE *distinctness_axioms(void)
{
  char *buf = NULL;
  size_t cap = 0, m = 0;
  int i, j;
  FILE *mem;

  for (i = 0; i < Distinct_count; i++)
    for (j = i + 1; j < Distinct_count; j++) {
      char line[128];
      int len = snprintf(line, sizeof(line),
			 "fof(tptp_do_ax_%d_%d, axiom, %s%d != %s%d).\n",
			 i, j, DO_PREFIX, i, DO_PREFIX, j);
      if (m + len + 1 > cap) {
	cap = (cap == 0 ? 1 << 20 : cap * 2) + len;
	buf = realloc(buf, cap);
	if (buf == NULL) fatal_error("distinctness_axioms, out of memory");
      }
      memcpy(buf + m, line, len);
      m += len;
    }
  if (buf == NULL) { buf = malloc(1); m = 0; }
  mem = fmemopen(buf, m, "r");
  if (mem == NULL)
    fatal_error("distinctness_axioms, fmemopen failed");
  return mem;
}

/*************
 *
 *   read_tptp_input()
 *
 *************/

/* TPTP's include has two forms:
 *
 *     include('Axioms/FOO.ax').               the whole file
 *     include('Axioms/FOO.ax',[ia1,ia3]).     only the named formulas
 *
 * The second argument used to be ignored, so a selective include pulled in
 * the whole file -- quietly *adding* axioms the problem had excluded, which
 * can make an unprovable conjecture provable.  <names> below is that list, or
 * NULL for the whole file.
 */

static
BOOL name_selected(Term name, Term names)
{
  if (names == NULL)
    return TRUE;
  while (cons_term(names)) {
    if (term_ident(ARG(names,0), name))
      return TRUE;
    names = ARG(names,1);
  }
  return FALSE;
}  /* name_selected */

static
void read_tptp_input(FILE *fin, FILE *fout, FILE *ferr,
		     Plist *assumps, Plist *goals, Term names)
{
  Term t = read_term(fin, fout);

  while (t != NULL) {

    if (is_term(t, "include", 1) || is_term(t, "include", 2)) {
      char *fname = new_str_copy(sn_to_str(SYMNUM(ARG(t,0))));
      char *fname2 = fname;
      Term sel = (is_term(t, "include", 2) ? ARG(t,1) : NULL);
      FILE *fin2;

      /* A selection applies to the formulas of the named file.  What it means
	 for files that file goes on to include is not clear from the TPTP
	 grammar, so refuse rather than guess: silently resolving it either way
	 would change which axioms the problem has. */
      if (names != NULL)
	fatal_error("read_tptp_input: include inside a selective include "
		    "is not supported");

      if (*fname2 == '\'' || *fname2 == '"') {
	fname2[strlen(fname2)-1] = '\0';
	fname2++;
      }
      fin2 = fopen(fname2, "r");
      if (fin2 == NULL) {
	fatal_file_not_found("read_tptp_input", fname2);
      }
      fprintf(fout, "\n%% Including file %s\n\n", fname2);
      {
	FILE *rew = rewrite_distinct_objects(fin2);
	fclose(fin2);
	read_tptp_input(rew, fout, ferr, assumps, goals, sel);
	fclose(rew);
      }
      free(fname);
    }
    else if (is_term(t, "cnf", 3) || is_term(t, "cnf", 4) ||
	     is_term(t, "fof", 3) || is_term(t, "fof", 4) ||
	     is_term(t, "input_clause", 3)) {
      if (name_selected(ARG(t,0), names)) {
	Formula form = tptp_input_to_ladr_formula(t);
	if (is_term(ARG(t,1), "conjecture", 0))
	  *goals = plist_prepend(*goals, form);
	else
	  *assumps = plist_prepend(*assumps, form);
      }
    }
    else {
      p_term(t);
      fatal_error("read_tptp_input: unknown term");
    }
    zap_term(t);
    t = read_term(fin, fout);
  }
}  /* read_tptp_input */

int main(int argc, char **argv)
{
  Plist assumps = NULL;
  Plist goals = NULL;
  Plist all = NULL;
  Plist p;

  Ilist fp_syms = NULL;
  I2list map;
  int i;

  BOOL quote_bad_ladr_syms = string_member("-q", argv, argc);

  init_standard_ladr();
  i = register_attribute("label",  STRING_ATTRIBUTE);

  clear_parse_type_for_all_symbols();
  declare_tptp_input_types();
  set_quote_char('\'');  /* TPTP uses single quotes for strings. */

  {
    FILE *rew = rewrite_distinct_objects(stdin);
    read_tptp_input(rew, stdout, stderr, &assumps, &goals, NULL);
    fclose(rew);
  }

  /* Distinct objects are pairwise unequal by definition; say so.  One object
     needs nothing said about it, and the generated names must not collide
     with anything the input already used. */

  if (Distinct_count > 0) {
    if (Do_prefix_in_input)
      fatal_error("tptp_to_ladr: the input already uses the name prefix "
		  DO_PREFIX ", which is what distinct objects are renamed to");
    printf("\n%% The TPTP formulas contain %d distinct object(s) (the \"...\"\n"
	   "%% token).  LADR has no such notion, so each one became a constant\n"
	   "%% named " DO_PREFIX "<n>, and their pairwise inequality -- which TPTP\n"
	   "%% leaves implicit -- is stated explicitly below, as %d disequalities.\n",
	   Distinct_count, (Distinct_count * (Distinct_count - 1)) / 2);
    if (Distinct_count > 1) {
      FILE *ax = distinctness_axioms();
      read_tptp_input(ax, stdout, stderr, &assumps, &goals, NULL);
      fclose(ax);
    }
  }

  assumps = reverse_plist(assumps);
  goals = reverse_plist(goals);

  /* Collect all of the LADR terms into one list. */

  all = NULL;
  all = plist_cat2(all, assumps);
  all = plist_cat2(all, goals);

  /* Collect all of the function and predicate symbols. */

  {
    I2list f = NULL;
    I2list r = NULL;
    gather_symbols_in_formulas(all, &r, &f);
    Ilist rsyms = multiset_to_set(r);
    Ilist fsyms = multiset_to_set(f);
    zap_i2list(f);
    zap_i2list(r);
    fp_syms = ilist_cat(rsyms, fsyms);
  }

  /* Determine which symbols are bad for LADR and create a map to new ones. */

  map = map_for_bad_ladr_syms(fp_syms, quote_bad_ladr_syms);

  /* Replace the bad symbols. */

  if (map != NULL) {
    I2list b;
    printf("\n%% The TPTP formulas contain function or predicate symbols\n"
	   "%% that are not legal LADR symbols, and we have replaced those\n"
	   "%% symbols with new symbols.  Here is the list of the unaccepted\n"
	   "%% symbols and the corresponding replacements.\n%%\n");
    for (b = map; b; b = b->next)
      printf("%%   (arity %d) %8s    %s\n",
	     sn_to_arity(b->j),
	     sn_to_str(b->i),
	     sn_to_str(b->j));
    printf("\n");
    for (p = assumps; p; p = p->next) {
      Term t = formula_to_term(p->v);
      Term t2 = replace_bad_syms_term(t, map);
      p->v = term_to_formula(t2);
    }
    for (p = goals; p; p = p->next) {
      Formula f = p->v;
      Term t = formula_to_term(f);
      zap_formula(f);
      t = replace_bad_syms_term(t, map);
      p->v = term_to_formula(t);
      zap_term(t);
    }
  }

  /* Change settings for LADR output. */  

  clear_parse_type_for_all_symbols();
  declare_standard_parse_types();  /* defaults for LADR */

  /* Output the LADR terms. */

  printf("set(prolog_style_variables).\n");

  /* The distinctness axioms are negative unit clauses, and prover9's
     auto_denials heuristic takes a Horn set with several negative clauses to
     be a set of candidate goals: it raises max_proofs to their number and
     then reports SEARCH FAILED when it proves the real goal but not the rest.
     On five distinct objects that turns a one-step proof into an apparent
     failure.  The axioms are not denials, so say so. */

  if (Distinct_count > 1)
    printf("clear(auto_denials).  %% the %d distinctness axioms below are not goals\n",
	   (Distinct_count * (Distinct_count - 1)) / 2);
  fwrite_formula_list(stdout, assumps, "assumptions");
  fwrite_formula_list(stdout, goals, "goals");

  exit(0);
}  /* main */

