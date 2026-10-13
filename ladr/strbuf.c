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

#include "strbuf.h"

/*
 * private definitions and types
 */

/* A String_buf is a flat, contiguous, growable array of characters.
 *
 * It used to be a singly-linked list of 100-byte chunks, which made every
 * indexed access walk the list from the head:
 *
 *     char sb_char(String_buf sb, int n) {
 *       Chunk h = sb->first;
 *       for (i = 0; i < n / CHUNK_SIZE; i++)    <-- n/100 pointer hops
 *         h = h->next;
 *       return h->s[n % CHUNK_SIZE];
 *     }
 *
 * Four places index a String_buf in a loop, so each was quadratic in the
 * buffer's length: the tokeniser in parse.c (about twenty sb_char call sites,
 * read sequentially), sb_remove_some_space() in the same file (which also
 * calls sb_replace_char, an identical walk), and both sb_to_malloc_string()
 * and sb_to_malloc_char_array().  Reading N characters cost about N^2/200
 * pointer hops -- for a megabyte of input, five thousand million.
 *
 * A cursor cacheing the last chunk reached would not have been enough.
 * parse.c's sb_remove_some_space() scans forward but reads a +/-2 window
 * around i, and the chunks were singly linked, so stepping back across a
 * chunk boundary had to restart from the head.  That happens once per hundred
 * characters, which leaves the cost quadratic, merely ten thousand times
 * smaller.  Doubly linking the chunks would have fixed it for eight bytes per
 * hundred-byte chunk, but there is no longer any reason to have chunks at
 * all: they exist because the original avoided realloc.
 *
 * So indexing is now sb->s[n], and the capacity doubles on growth, which
 * keeps appending amortised O(1) per character -- the property the "last"
 * pointer was added to recover.
 *
 * The characters are allocated through LADR's own allocator rather than
 * malloc, deliberately.  Chunks came from get_cmem(), so String_buf memory
 * counted towards max_megs; keeping it there means neither that accounting
 * nor the memory report quietly changes meaning.
 */

struct string_buf {
  char *s;         /* the characters, contiguous; NULL when capacity is 0 */
  int size;        /* how many are in use */
  int capacity;    /* how many are allocated; always a whole number of
		      pointers, because that is what get_mem deals in */
};

#define SB_INITIAL_CAPACITY 64

/*
 * memory management
 */

#define PTRS_STRING_BUF PTRS(sizeof(struct string_buf))
static unsigned String_buf_gets, String_buf_frees;
static unsigned Buffer_gets, Buffer_frees;
static unsigned Buffer_bytes;    /* currently allocated for characters */

/*************
 *
 *   sb_ensure_capacity()
 *
 *************/

/* Make room for at least <needed> characters, doubling so that a run of
   appends costs amortised O(1) each.  get_mem/free_mem deal in pointers, so
   the capacity is rounded up to a whole number of them, and the rounded
   figure is what gets remembered -- free_mem must be given the same count
   that get_mem was. */

static
void sb_ensure_capacity(String_buf sb, int needed)
{
  if (needed <= sb->capacity)
    return;
  else {
    int new_capacity = (sb->capacity == 0 ? SB_INITIAL_CAPACITY : sb->capacity);
    char *new_s;

    while (new_capacity < needed) {
      if (new_capacity > INT_MAX / 2)
	fatal_error("sb_ensure_capacity, String_buf too big");
      new_capacity *= 2;
    }
    new_capacity = PTRS(new_capacity) * BYTES_POINTER;

    new_s = (char *) get_mem(PTRS(new_capacity));
    Buffer_gets++;
    Buffer_bytes += new_capacity;

    if (sb->s != NULL) {
      memcpy(new_s, sb->s, sb->size);
      free_mem(sb->s, PTRS(sb->capacity));
      Buffer_frees++;
      Buffer_bytes -= sb->capacity;
    }
    sb->s = new_s;
    sb->capacity = new_capacity;
  }
}  /* sb_ensure_capacity */

/*************
 *
 *    String_buf get_string_buf()
 *
 *************/

/* DOCUMENTATION
This routine allocates and returns an empty String_buf.
*/

/* PUBLIC */
String_buf get_string_buf(void)
{
  String_buf p = get_cmem(PTRS_STRING_BUF);   /* zeroed: s=NULL, size=0 */
  String_buf_gets++;
  return(p);
}  /* get_string_buf */

/*************
 *
 *    free_string_buf()
 *
 *************/

static
void free_string_buf(String_buf p)
{
  free_mem(p, PTRS_STRING_BUF);
  String_buf_frees++;
}  /* free_string_buf */

/*************
 *
 *   fprint_strbuf_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to FILE *fp) memory usage statistics for data types
associated with the strbuf package.
The Boolean argument heading tells whether to print a heading on the table.
*/

/* PUBLIC */
void fprint_strbuf_mem(FILE *fp, BOOL heading)
{
  int n;
  if (heading)
    fprintf(fp, "  type (bytes each)        gets      frees     in use      bytes\n");

  /* This line used to count 100-byte chunks.  There are no chunks now, so it
     reports the character buffers instead: how many have been allocated and
     released, and how many bytes they currently hold. */

  fprintf(fp, "sb buffer (  var)   %11u%11u%11u%9.1f K\n",
	  Buffer_gets, Buffer_frees,
	  Buffer_gets - Buffer_frees,
	  Buffer_bytes / 1024.);

  n = sizeof(struct string_buf);
  fprintf(fp, "string_buf (%4d)   %11u%11u%11u%9.1f K\n",
          n, String_buf_gets, String_buf_frees,
          String_buf_gets - String_buf_frees,
          ((String_buf_gets - String_buf_frees) * n) / 1024.);

}  /* fprint_strbuf_mem */

/*************
 *
 *   p_strbuf_mem()
 *
 *************/

/* DOCUMENTATION
This routine prints (to stdout) memory usage statistics for data types
associated with the strbuf package.
*/

/* PUBLIC */
void p_strbuf_mem()
{
  fprint_strbuf_mem(stdout, TRUE);
}  /* p_strbuf_mem */

/*
 *  end of memory management
 */

/*************
 *
 *    String_buf init_string_buf()
 *
 *************/

/* DOCUMENTATION
This routine allocates and returns a String_buf, initialized
to string s.  Don't forget to call zap_string_buf(sb) when
finished with it.
Also see get_string_buf().
*/

/* PUBLIC */
String_buf init_string_buf(char *s)
{
  String_buf p = get_string_buf();
  sb_append(p, s);
  return(p);
}  /* init_string_buf */

/*************
 *
 *   fprint_sb()
 *
 *************/

/* DOCUMENTATION
This routine prints String_buf sb to FILE *fp.
*/

/* PUBLIC */
void fprint_sb(FILE *fp, String_buf sb)
{
  int i;
  for (i = 0; i < sb->size; i++)
    if (sb->s[i] != '\0')      /* NUL characters are skipped, as before */
      putc(sb->s[i], fp);
}  /* fprint_sb */

/*************
 *
 *   p_sb()
 *
 *************/

/* DOCUMENTATION
This routine prints String_buf sb, followed by '\n' and fflush, to stdout.
If you don't want the newline, use fprint_sb() instead.
*/

/* PUBLIC */
void p_sb(String_buf sb)
{
  fprint_sb(stdout, sb);
  printf("\n");
  fflush(stdout);
}  /* p_sb */

/*************
 *
 *   sb_append()
 *
 *************/

/* DOCUMENTATION
This routine appends string s to String_buf sb.
The NULL character that marks the end of s does not go into
the String_buf.
*/

/* PUBLIC */
void sb_append(String_buf sb, char *s)
{
  int n = (int) strlen(s);
  if (n > 0) {
    sb_ensure_capacity(sb, sb->size + n);
    memcpy(sb->s + sb->size, s, n);
    sb->size += n;
  }
}  /* sb_append */

/*************
 *
 *   sb_append_char()
 *
 *************/

/* DOCUMENTATION
This routine appends character c to String_buf sb.
*/

/* PUBLIC */
void sb_append_char(String_buf sb, char c)
{
  /* Note that c may be '\0'.  The first version of this routine built a
     one-character string and called sb_append, which cannot append a NUL,
     and that matters because this package is also used for sequences of
     small integers. */

  sb_ensure_capacity(sb, sb->size + 1);
  sb->s[sb->size] = c;
  sb->size++;
}  /* sb_append_char */

/*************
 *
 *   sb_replace_char()
 *
 *************/

/* DOCUMENTATION
This routine replaces a character in a String_buf.
If the index i is out of range, nothing happens.
*/

/* PUBLIC */
void sb_replace_char(String_buf sb, int i, char c)
{
  if (i < 0 || i >= sb->size)
    return;
  else
    sb->s[i] = c;
}  /* sb_replace_char */

/*************
 *
 *   sb_char()
 *
 *************/

/* DOCUMENTATION
This routine returns the n-th character (counting from 0) of String_buf sb.
If index n is out of range, the NULL character '\0' is returned.
*/

/* PUBLIC */
char sb_char(String_buf sb, int n)
{
  if (n < 0 || n >= sb->size)
    return '\0';
  else
    return sb->s[n];
}  /* sb_char */

/*************
 *
 *   sb_cat_copy()
 *
 *************/

/* DOCUMENTATION
This routine appends a copy of String_buf sb2 to the end of String_buf sb1.
String_buf sb2 is not changed.
You can use sb_cat() instead if you won't be needing sb2.
*/

/* PUBLIC */
void sb_cat_copy(String_buf sb1, String_buf sb2)
{
  /* Copying stops at the end of sb2 or at the first NUL character in it,
     whichever comes first -- the same rule as before. */
  int n = 0;
  while (n < sb2->size && sb2->s[n] != '\0')
    n++;
  if (n > 0) {
    sb_ensure_capacity(sb1, sb1->size + n);
    memcpy(sb1->s + sb1->size, sb2->s, n);
    sb1->size += n;
  }
}  /* sb_cat_copy */

/*************
 *
 *   sb_cat()
 *
 *************/

/* DOCUMENTATION
This routine appends a copy of sb2 to sb1, then deallocates sb2.
Do not refer to sb2 after calling this rouine because it won't exist.
You can use sb_cat_copy() instead if you need to save sb2.
*/

/* PUBLIC */
void sb_cat(String_buf sb1, String_buf sb2)
{
  sb_cat_copy(sb1, sb2);
  zap_string_buf(sb2);
}  /* sb_cat */

/*************
 *
 *   zap_string_buf()
 *
 *************/

/* DOCUMENTATION
This routine deallocates a String_buf and frees all memory
associated with it.
*/

/* PUBLIC */
void zap_string_buf(String_buf sb)
{
  if (sb->s != NULL) {
    free_mem(sb->s, PTRS(sb->capacity));
    Buffer_frees++;
    Buffer_bytes -= sb->capacity;
  }
  free_string_buf(sb);
}  /* zap_string_buf */

/*************
 *
 *   sb_to_malloc_string()
 *
 *************/

/* DOCUMENTATION
This routine returns a new, ordinary C string corresponding to the
String_buf argument sb.  WARNING: the new string, say s, is
dynamically allocated (malloced), so don't forget to call
free(s) when you are finished with the string.  (This routine
is not intended for printing String_bufs; use fprint_sb() instead.)
<P>
String_bufs do not have a NULL character marking the end;
instead, they keep a count of the number of characters.
<p>
If the String_buf contains NULL characters, they do NOT mark the
end of the string.  Instead, they are simply ignored when constructing
the ordinary string.
*/

/* PUBLIC */
char *sb_to_malloc_string(String_buf sb)
{
  char *s = malloc(sb->size + 1);

  if (s == NULL)
    return NULL;
  else {
    int j = 0;  /* index for new string */
    int i;      /* index for String_buf */
    for (i = 0; i < sb->size; i++)
      if (sb->s[i] != '\0')
	s[j++] = sb->s[i];
    s[j] = '\0';
    return s;
  }
}  /* sb_to_malloc_string */

/*************
 *
 *   sb_to_malloc_char_array()
 *
 *************/

/* DOCUMENTATION
This routine is similar to sb_to_malloc_string(), except that
null characters are copied to the new string.
*/

/* PUBLIC */
char *sb_to_malloc_char_array(String_buf sb)
{
  char *s = malloc(sb->size + 1);

  if (s == NULL)
    return NULL;
  else {
    if (sb->size > 0)
      memcpy(s, sb->s, sb->size);
    s[sb->size] = '\0';
    return s;
  }
}  /* sb_to_malloc_char_array */

/*************
 *
 *   sb_size()
 *
 *************/

/* DOCUMENTATION
*/

/* PUBLIC */
int sb_size(String_buf sb)
{
  return sb->size;
}  /* sb_size */

/*************
 *
 *   sb_append_int()
 *
 *************/

/* DOCUMENTATION
Convert an integer to a string and append the string to a String_buf.
*/

/* PUBLIC */
void sb_append_int(String_buf sb, int i)
{
  char s[25];
  sb_append(sb, int_to_str(i, s, 25));
}  /* sb_append_int */
