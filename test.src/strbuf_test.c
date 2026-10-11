/* Exercise the String_buf package: mixed appends across chunk boundaries,
   concatenation, indexing, replacement, and a timing check for the
   quadratic-append fix. */
#include "../ladr/strbuf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int failures = 0;

static void check(const char *what, int ok)
{
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else       printf("ok:   %s\n", what);
}

int main(void)
{
  /* 1. char-by-char across many chunk boundaries (CHUNK_SIZE is 100) */
  {
    String_buf sb = get_string_buf();
    char *s;
    int i, ok = 1;
    for (i = 0; i < 1000; i++)
      sb_append_char(sb, 'a' + (i % 26));
    check("size after 1000 sb_append_char", sb_size(sb) == 1000);
    s = sb_to_malloc_string(sb);
    for (i = 0; i < 1000; i++)
      if (s[i] != 'a' + (i % 26)) { ok = 0; break; }
    check("content after 1000 sb_append_char", ok && strlen(s) == 1000);
    for (i = 0; i < 1000; i++)
      if (sb_char(sb, i) != 'a' + (i % 26)) { ok = 0; break; }
    check("sb_char agrees", ok);
    free(s); zap_string_buf(sb);
  }

  /* 2. interleaved sb_append / sb_append_char (the case where a stale
        "last" pointer would corrupt the buffer) */
  {
    String_buf sb = init_string_buf("start");
    char *s;
    int i;
    for (i = 0; i < 200; i++) {
      sb_append(sb, "0123456789");
      sb_append_char(sb, '|');
    }
    sb_append(sb, "end");
    s = sb_to_malloc_string(sb);
    check("interleaved size", sb_size(sb) == 5 + 200 * 11 + 3);
    check("interleaved prefix", strncmp(s, "start0123456789|", 16) == 0);
    check("interleaved suffix", strcmp(s + strlen(s) - 3, "end") == 0);
    check("interleaved strlen", strlen(s) == (size_t) sb_size(sb));
    free(s); zap_string_buf(sb);
  }

  /* 3. sb_cat_copy across chunk boundaries */
  {
    String_buf a = init_string_buf("A");
    String_buf b = get_string_buf();
    char *s;
    int i;
    for (i = 0; i < 350; i++)
      sb_append_char(b, '0' + (i % 10));
    sb_cat_copy(a, b);
    sb_append(a, "Z");
    s = sb_to_malloc_string(a);
    check("cat_copy size", sb_size(a) == 1 + 350 + 1);
    check("cat_copy content", s[0] == 'A' && s[1] == '0' && s[350] == '9'
                              && s[351] == 'Z');
    check("cat_copy left b alone", sb_size(b) == 350);
    free(s); zap_string_buf(a); zap_string_buf(b);
  }

  /* 4. sb_cat_copy still stops at an embedded NUL, as the original did */
  {
    String_buf a = get_string_buf();
    String_buf b = get_string_buf();
    sb_append(b, "abc");
    sb_append_char(b, '\0');
    sb_append(b, "def");
    sb_cat_copy(a, b);
    check("cat_copy stops at embedded NUL", sb_size(a) == 3);
    zap_string_buf(a); zap_string_buf(b);
  }

  /* 5. sb_replace_char */
  {
    String_buf sb = get_string_buf();
    char *s;
    int i;
    for (i = 0; i < 250; i++)
      sb_append_char(sb, 'x');
    sb_replace_char(sb, 0,   'A');
    sb_replace_char(sb, 150, 'B');
    sb_replace_char(sb, 249, 'C');
    s = sb_to_malloc_string(sb);
    check("sb_replace_char", s[0]=='A' && s[150]=='B' && s[249]=='C'
                             && s[1]=='x' && sb_size(sb)==250);
    free(s); zap_string_buf(sb);
  }

  /* 6. appending is no longer quadratic: 400k chars must not take long */
  {
    String_buf sb = get_string_buf();
    clock_t t0 = clock();
    int i;
    double secs;
    for (i = 0; i < 400000; i++)
      sb_append_char(sb, 'q');
    secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("      400000 sb_append_char took %.2f s\n", secs);
    check("400000 appends under 2s (was quadratic)", secs < 2.0);
    check("400000 appends size", sb_size(sb) == 400000);
    zap_string_buf(sb);
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures != 0;
}
