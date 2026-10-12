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

#include "fatal.h"

/* Private definitions and types */

static int Fatal_exit_code = 1;

/*************
 *
 *   bell()
 *
 *************/

/* DOCUMENTATION
Send the bell character '\007' to a file.
*/

/* PUBLIC */
void bell(FILE *fp)
{
  fprintf(fp, "%c", '\007');
}  /* bell */

/*************
 *
 *   get_fatal_exit_code()
 *
 *************/

/* DOCUMENTATION
This function returns the exit code that will be used in
case fatal_error() is called.
*/

/* PUBLIC */
int get_fatal_exit_code()
{
  return Fatal_exit_code;
}  /* get_fatal_exit_code */

/*************
 *
 *   set_fatal_exit_code()
 *
 *************/

/* DOCUMENTATION
This routine changes the exit code that will be used in case
fatal_error() is called.  The default value is 1.
*/

/* PUBLIC */
void set_fatal_exit_code(int exit_code)
{
  Fatal_exit_code = exit_code;
}  /* set_fatal_exit_code */

/*************
 *
 *   fatal_error()
 *
 *************/

/* DOCUMENTATION
This routine should be called if something terrible happens.
The message is printed to stdout and to stderr, and the
process exits with the fatal_exit_code (default 1).
*/

/* PUBLIC */
void fatal_error(char *message)
{
  fprintf(stdout, "\nFatal error:  %s\n\n", message);
  fprintf(stderr, "\nFatal error:  %s\n\n", message);
  exit(Fatal_exit_code);
}  /* fatal_error */

/*************
 *
 *   fatal_file_not_found()
 *
 *************/

/* DOCUMENTATION
Report that <filename> could not be opened, and exit.  If <context> is
non-NULL it names the routine that was trying to open the file.

The filename comes from input or from argv, so its length is not bounded by
anything this code controls.  The message is therefore measured and then
allocated to fit, rather than formatted into a fixed buffer: snprintf()
returns the length the complete message would need, so nothing is truncated
however long the name is, and no size has to be guessed.

Two deliberate choices.  The allocation is plain malloc(), not the LADR
allocator: palloc() calls fatal_error() when it cannot allocate, which from
here would recurse.  And the message is built in this routine rather than in
each caller, so that no caller carries a large buffer in its frame -- one of
them (read_tptp_input, which follows TPTP include() directives) is recursive,
where that cost would be paid per nesting level.

If the allocation fails -- we may be here precisely because memory ran out --
a fixed buffer of FILE_MESSAGE_SIZE is used instead, which reports a name
longer than that truncated but still says what went wrong.

The message cannot be freed, because fatal_error() does not return, so the
pointer is held in a file-scope variable rather than a local.  That is not
cosmetic: an allocation still reachable at exit() is not a leak, and holding
it this way keeps the program clean under LeakSanitizer, whereas a local
would make every run that reports a missing file look like it leaked.
*/

static char *Fatal_message = NULL;   /* lives until exit; see below */

/* PUBLIC */
void fatal_file_not_found(char *context, char *filename)
{
  char *ctx = (context == NULL ? "" : context);
  char *sep = (context == NULL ? "" : ", ");
  int n = snprintf(NULL, 0, "%s%sfile %s not found", ctx, sep, filename);

  if (n >= 0) {
    Fatal_message = malloc((size_t) n + 1);
    if (Fatal_message != NULL) {
      snprintf(Fatal_message, (size_t) n + 1,
	       "%s%sfile %s not found", ctx, sep, filename);
      fatal_error(Fatal_message);   /* does not return */
    }
  }

  {
    char s[FILE_MESSAGE_SIZE];
    snprintf(s, sizeof(s), "%s%sfile %s not found", ctx, sep, filename);
    fatal_error(s);
  }
}  /* fatal_file_not_found */
