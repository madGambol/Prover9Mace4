#!/bin/bash
#------------------------------------------------------------------------------
# A measurement, not a pass/fail test: how long tptp_to_ladr takes on a single
# formula as that formula grows.
#
# THE DEFECT
#   A String_buf was a singly-linked list of 100-byte chunks, and both of its
#   indexed accessors walked that list from the head:
#
#       char sb_char(String_buf sb, int n) {
#         Chunk h = sb->first;
#         for (i = 0; i < n / CHUNK_SIZE; i++)    <-- n/100 pointer hops
#           h = h->next;
#         return h->s[n % CHUNK_SIZE];
#       }
#
#   sb_replace_char() was character-for-character the same walk.  The struct
#   already had a "last" pointer so that APPENDING would not rescan; reading
#   had been left alone.
#
#   Four places index a String_buf in a loop, so each was quadratic in the
#   buffer's length:
#
#     - the tokeniser in ladr/parse.c, about twenty sb_char call sites reading
#       sequentially;
#     - sb_remove_some_space() in the same file, which also calls
#       sb_replace_char;
#     - sb_to_malloc_string() and sb_to_malloc_char_array(), which is every
#       conversion of a buffer to an ordinary C string.
#
#   Reading N characters cost about N^2/200 pointer hops.  For a megabyte of
#   input that is five thousand million.
#
# WHY A CURSOR WAS NOT THE FIX
#   The obvious repair is to cache the last chunk reached and resume from it.
#   That is not enough here.  sb_remove_some_space() scans forward but reads a
#   +/-2 window around i:
#
#       sb_char(sb, i)  sb_char(sb, i-1)  sb_char(sb, i-2)
#                       sb_char(sb, i+1)  sb_char(sb, i+2)
#
#   and the chunks were singly linked, so stepping back across a chunk
#   boundary had to restart from the head -- once per hundred characters.
#   That leaves the cost quadratic, merely ten thousand times smaller:
#   (N/100)^2/2 rather than N^2/200.  Doubly linking the chunks would have
#   fixed it for eight bytes per hundred-byte chunk, but chunks only existed
#   because the original avoided realloc.
#
# THE FIX
#   A String_buf is now a flat, contiguous, growable array: indexing is
#   sb->s[n], and the capacity doubles on growth so appending stays amortised
#   O(1).  About a hundred lines of chunk machinery went away.  The characters
#   are still allocated through LADR's own allocator rather than malloc, so
#   String_buf memory still counts towards max_megs and the memory report does
#   not change meaning.
#
# MEASURED, translating one formula of n disjuncts
#
#       n          chunk list    flat buffer
#       2000          47 ms         45 ms
#       4000         135 ms         95 ms
#       8000         704 ms        324 ms
#      16000        3557 ms       1526 ms      2.3x
#
#   And in the callgrind profile of a 12,000-disjunct formula, sb_char went
#   from 28.78% of all instructions to absent, with the total falling from
#   4.39 to 3.13 thousand million instructions -- exactly its old share.
#
# WHAT IS STILL SUPERLINEAR, AND WHY THIS IS NOT THE WHOLE STORY
#   Translation is still superlinear: doubling n still multiplies the time by
#   about five.  Two other costs now dominate the profile, and they are
#   separate defects, not this one:
#
#       multiset_add_n   46%   a non-tail-recursive linear list scan, called
#                              once per symbol OCCURRENCE, so quadratic in the
#                              number of distinct symbols.  Its recursion
#                              depth is the list length, which is also why
#                              very large inputs overflow the stack.
#       flatten_top      25%   flattening a long chain of one connective
#       memset           18%   zeroing from calloc of growing arrays
#
#   This file documents the sb_char fix only.  Fixing those is separate work.
#
# RUN IT
#   test.src/demos/perf-strbuf-indexing.sh [bin-directory]
#------------------------------------------------------------------------------
B="${1:-$(cd "$(dirname "$0")/../.." && pwd)/bin}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
echo "translating one formula of n disjuncts with $B/tptp_to_ladr:"
for n in 2000 4000 8000 16000; do
  python3 -I - "$n" > "$W/f.p" <<'PY'
import sys
n = int(sys.argv[1])
print("fof(a,axiom,")
print(" | ".join(f"p{i}(a)" for i in range(n)))
print(").")
PY
  s=$(date +%s%3N)
  ( ulimit -t 300; "$B/tptp_to_ladr" < "$W/f.p" > "$W/out" 2>/dev/null )
  printf '  n=%-6s %7s ms   formulas out: %s\n' "$n" "$(( $(date +%s%3N) - s ))" \
    "$(grep -c 'label(a)' "$W/out" 2>/dev/null)"
done
echo "(the chunked version: 47, 135, 704, 3557 ms)"
