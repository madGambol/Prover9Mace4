#!/bin/bash

#
#  Copyright (C) 2026 Chris Arena, 2026-10-27.
#  Written in collaboration with Claude Opus 5.5 (Anthropic).
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written to re-run the reproducers that
#  FIXES.md describes against other LADR builds, and carries the
#  library's licence so that it can be distributed with it.
#
#  This file is part of the LADR Deduction Library.
#
#  The LADR Deduction Library is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License,
#  version 2.
#
#  The LADR Deduction Library is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the LADR Deduction Library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

# run_all.sh -- re-run the crash / wrong-answer reproducers from
# ../FIXES.md against any LADR bin/ directory.
#
#   usage: run_all.sh BIN_DIR [LABEL] [CASE ...]
#
#   BIN_DIR  a bin/ holding prover9, mace4, prooftrans, ... (any build)
#   LABEL    name of the log directory, logs/LABEL/   (default: parent dir name)
#   CASE     run only these case ids (default: every case valid for the build)
#
# The build type is detected from the binaries:
#   ASan/UBSan build (prover9 references __asan)   -> sanitizer-only cases
#   poisoned build   (prover9 has __wrap_free_mem) -> poison-only cases (M8)
#   anything else                                  -> all ordinary cases
# Unit-level cases (T9, PF5, M13) compile the old project's tests against
# BIN_DIR/../ladr/libladr.a, so they run only when that library exists.
#
# Writes logs/LABEL/<case>.{cmd,out,err} and logs/LABEL/summary.tsv:
#   case  verdict  rc  secs  class  note
# verdict is one of: pass | crash | abort | hang | wrong | fatal | sanitizer |
#                    oom | n/a
#
# Every run is wrapped in `timeout`, core dumps are off, and plain builds get a
# 16 GB ulimit -v (ASan builds cannot take one; their cases are all small).
# Cases run one at a time.  Nothing here writes outside this directory.

S=$(cd "$(dirname "$0")" && pwd)
C=$S/cases
[ -n "${1:-}" ] || { sed -n 2,26p "$0"; exit 2; }
B=$(cd "$1" && pwd) || exit 2
LABEL=${2:-$(basename "$(dirname "$B")")}
shift; [ $# -gt 0 ] && shift
L=$S/logs/$LABEL
W=$L/work
rm -rf "$L"; mkdir -p "$W"
TREE=$(dirname "$B")
VMEM=16000000                      # KB, = ~16 GB
ASAN=0;   nm "$B/prover9" 2>/dev/null | grep -q __asan          && ASAN=1
POISON=0; nm "$B/prover9" 2>/dev/null | grep -q __wrap_free_mem && POISON=1
NEW=0;    timeout 10 "$B/prover9" -help </dev/null 2>&1 | grep -q 'problem.p' && NEW=1
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1
export PATH="$B:$PATH"
SUM=$L/summary.tsv
printf 'case\tverdict\trc\tsecs\tclass\tnote\n' > "$SUM"
{ echo "bin=$B"; echo "asan=$ASAN poison=$POISON new_tptp_reader=$NEW"; date; } > "$L/build.txt"

LONG=$(printf 'z%.0s' $(seq 1 300))
LONG4000=$(awk 'BEGIN{s="";while(length(s)<4000)s=s "yyyyyyyyyy";print substr(s,1,4000)}')

# x NAME TIMEOUT VMEM STDIN CWD cmd... -- run one command, set RC SECS
x() {
  local name=$1 t=$2 vm=$3 in=$4 cwd=$5; shift 5
  [ "$ASAN" = 1 ] && vm=0
  printf '%q ' "$@" > "$L/$name.cmd"; printf ' < %s   (cwd %s, timeout %ss, vmem %s KB)\n' "$in" "$cwd" "$t" "$vm" >> "$L/$name.cmd"
  local t0; t0=$(date +%s.%N)
  ( cd "$cwd" || exit 99; ulimit -c 0; [ "$vm" != 0 ] && ulimit -v "$vm"
    exec timeout -k 5 "$t" "$@" ) < "$in" > "$L/$name.out" 2> "$L/$name.err"
  RC=$?
  SECS=$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.2f", b-a}')
}
# o NAME: stdout+stderr of the last run of NAME
o() { cat "$L/$1.out" "$L/$1.err" 2>/dev/null; }
# cls NAME: classify how the last run of NAME ended
cls() {
  local all; all=$(o "$1")
  if   [ "$RC" = 124 ] || [ "$RC" = 137 ];                 then echo "hang(timeout)"
  elif grep -q 'ERROR: AddressSanitizer' <<<"$all";           then echo "ASan:$(grep -m1 -o 'AddressSanitizer: [a-z-]*' <<<"$all" | cut -d' ' -f2)"
  elif grep -q 'runtime error:' <<<"$all";                    then echo "UBSan:$(grep -m1 -o 'runtime error: [a-z -]*' <<<"$all" | cut -c16-60)"
  elif grep -q 'buffer overflow detected' <<<"$all";          then echo "SIGABRT(fortify)"
  elif grep -q 'stack smashing detected' <<<"$all";           then echo "SIGABRT(stack-protector)"
  elif grep -q 'Fatal glibc error: malloc' <<<"$all";         then echo "SIGABRT(glibc heap check)"
  elif grep -qE 'out of memory|Max_megs parameter exceeded' <<<"$all" && [ "$RC" -lt 100 ]; then echo "fatal:out of memory at cap"
  elif [ "$RC" = 134 ];                                     then echo "SIGABRT"
  elif [ "$RC" = 139 ];                                     then echo "SIGSEGV"
  elif [ "$RC" = 102 ] && grep -q 'SIGSEGV' <<<"$all";        then echo "SIGSEGV(handler,rc102)"
  elif [ "$RC" -gt 128 ] 2>/dev/null;                       then echo "signal$((RC-128))"
  elif grep -q 'operating system is out of memory' <<<"$all"; then echo "OOM(16GB cap)"
  elif grep -q 'Fatal error' <<<"$all";                       then echo "fatal:$(grep -m1 'Fatal error' <<<"$all" | sed 's/Fatal error: *//' | cut -c1-50)"
  else echo "rc=$RC"; fi
}
# rec ID PASSED(0/1) NAME NOTE -- write the summary row
rec() {
  local id=$1 ok=$2 name=$3 note=$4 c v
  c=$(cls "$name")
  if [ "$ok" = 1 ]; then v=pass
    case " $SANONLY " in *" $id "*) [ "$ASAN" = 0 ] && v="invisible" ;; esac
    case " $POIONLY " in *" $id "*) [ "$POISON" = 0 ] && v="invisible" ;; esac
  else case $c in
         hang*) v=hang ;; ASan*|UBSan*) v=sanitizer ;; SIGABRT*) v=abort ;;
         SIGSEGV*|signal*) v=crash ;; OOM*) v=oom ;; fatal*) v=fatal ;; *) v=wrong ;;
       esac
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$id" "$v" "$RC" "$SECS" "$c" "$note" >> "$SUM"
  printf '  %-7s %-9s rc=%-4s %6ss  %-28s %s\n' "$id" "$v" "$RC" "$SECS" "$c" "$note"
}
na() { printf '%s\tn/a\t-\t-\t-\t%s\n' "$1" "$2" >> "$SUM"; printf '  %-7s n/a       %s\n' "$1" "$2"; }
has() { o "$1" | grep -q -- "$2"; }
hasF() { o "$1" | grep -qF -- "$2"; }

# ---------------------------------------------------------------- cases
# Memory safety reachable from the command line / input (M1 M2 M3 M5 T1)
c_M1()  { x M1 30 $VMEM /dev/null "$W" "$B/mace4" -n 10000000 -f "$C/tiny.in"
          has M1 'DOMAIN SIZE 10000000'; rec M1 $((! $?)) M1 "mace4 -n 10000000: banner sprintf into char[20]"; }
c_M2()  { x M2 30 $VMEM /dev/null "$W" "$B/prover9" -f "$LONG"
          has M2 'not found'; rec M2 $((! $?)) M2 "prover9 -f <300-char missing name>"; }
c_M2b() { x M2b 30 $VMEM /dev/null "$W" "$B/mace4" -f "$LONG"
          has M2b 'not found'; rec M2b $((! $?)) M2b "mace4 -f <300-char missing name> (same read_all_input)"; }
c_M3a() { x M3a 30 $VMEM /dev/null "$W" "$B/prooftrans" -f "$LONG"
          has M3a 'not found'; rec M3a $((! $?)) M3a "prooftrans -f <300-char missing name>"; }
c_M3b() { x M3b 30 $VMEM /dev/null "$W" "$B/directproof" -f "$LONG"
          has M3b 'not found'; rec M3b $((! $?)) M3b "directproof -f <300-char missing name>"; }
c_M3c() { x M3c 30 $VMEM /dev/null "$W" "$B/interpformat" -f "$LONG"
          has M3c 'not found'; rec M3c $((! $?)) M3c "interpformat -f <300-char missing name>"; }
c_T1()  { x T1 30 $VMEM "$C/longinc.p" "$C" "$B/tptp_to_ladr"
          has T1 'not found'; rec T1 $((! $?)) T1 "tptp_to_ladr: include('<300 chars>') of a missing file"; }
c_M5()  { x M5 30 $VMEM "$C/proof.txt" "$W" "$B/prooftrans" hints -label "$LONG"
          has M5 "${LONG}_1"; rec M5 $((! $?)) M5 "prooftrans hints -label <300 chars> < proof"; }
# Format string
c_F1()  { x F1 30 $VMEM "$C/fmt.in" "$W" "$B/unfast"
          has F1 'term ends before period'; rec F1 $((! $?)) F1 "unfast < 'f(%s%s%s%s%s%n,a)'"; }
# Truncated / malformed proofs (D1 D2)
c_D1a() { x D1a 30 4000000 "$C/trunc.txt" "$W" "$B/prooftrans"
          [ "$RC" = 1 ]; rec D1a $((! $?)) D1a "prooftrans < proof truncated after '% Proof' (want clean exit 1; the bug loops forever, growing memory)"; }
c_D1b() { x D1b 30 4000000 "$C/trunc.txt" "$W" "$B/directproof"
          [ "$RC" = 1 ]; rec D1b $((! $?)) D1b "directproof < truncated proof (want clean exit 1; the bug loops forever, growing memory)"; }
c_D1c() { x D1c 30 $VMEM "$C/proof.txt" "$W" "$B/prooftrans" parents_only
          has D1c 'end of proof'; rec D1c $((! $?)) D1c "control: prooftrans parents_only on a complete proof"; }
c_D1d() { x D1d 30 $VMEM "$C/proof.txt" "$W" "$B/prooftrans"
          awk '/^% Given clauses/{g=NR} g&&NR>g&&/^$/{n++} g&&NR>g&&!/^$/{exit} END{exit !(n==1)}' "$L/D1d.out"
          rec D1d $((! $?)) D1d "D1 side effect: exactly 1 blank line after proof comments (2 = drain bug, cosmetic)"; }
c_D2a() { x D2a 30 4000000 "$C/longtok.txt" "$W" "$B/prooftrans"
          [ "$RC" = 1 ]; rec D2a $((! $?)) D2a "prooftrans < proof with 200,000-char clause id (want clean exit 1; the bug overflows char s[1000])"; }
c_D2b() { x D2b 30 4000000 "$C/longtok.txt" "$W" "$B/directproof"
          [ "$RC" = 1 ]; rec D2b $((! $?)) D2b "directproof < same (want clean exit 1; the bug overflows char s[1000])"; }
# Filename messages (T2), circular include (T3)
c_T2a() { x T2a 30 $VMEM "$C/inc4000.p" "$C" "$B/tptp_to_ladr"
          hasF T2a "file $LONG4000 not found"; rec T2a $((! $?)) T2a "tptp_to_ladr: 4000-char include name reported in full (usability)"; }
c_T2b() { x T2b 30 $VMEM /dev/null "$W" "$B/prover9" -f "$LONG4000"
          hasF T2b "read_all_input, file $LONG4000 not found"; rec T2b $((! $?)) T2b "prover9 -f <4000-char name> reported in full (usability)"; }
c_T3()  { x T3 120 $VMEM "$C/loop.p" "$C" "$B/tptp_to_ladr"
          [ "$RC" -lt 100 ] && [ "$RC" != 0 ] && ! has T3 'SIGSEGV'
          rec T3 $((! $?)) T3 "tptp_to_ladr on a self-including file: unbounded include() recursion (FIXES: crashes in ALL builds, T3 only deepened it)"; }
# TPTP translation correctness (T4 T5 T6 T7 T8, arity ceiling)
tcase() { # tcase ID FILE WANT(1=must prove, 0=must not prove) NOTE
  local id=$1
  x "$id" 60 $VMEM "$C/$2" "$C" "$B/tptp_to_ladr"
  if [ "$RC" != 0 ] || ! grep -q 'formulas(' "$L/$id.out"; then
    rec "$id" 0 "$id" "TRANSLATION FAILED: $4"; return; fi
  cp "$L/$id.out" "$W/$id.in"
  x "${id}p" 60 $VMEM /dev/null "$W" "$B/prover9" -f "$W/$id.in"
  if [ "$3" = 1 ]; then has "${id}p" 'THEOREM PROVED'
  else [ "$RC" -lt 100 ] && ! has "${id}p" 'THEOREM PROVED' && ! has "${id}p" 'Fatal error'; fi
  rec "$id" $((! $?)) "${id}p" "$4"; }
c_TR()  { tcase TR  grp.p     1 "control: tptp_to_ladr + prover9 prove a small group problem"; }
c_T4a() { x T4a 60 $VMEM "$C/neg.p" "$C" "$B/tptp_to_ladr"; hasF T4a '-(all X p(X))'
          rec T4a $((! $?)) T4a "'~ ! [X] : p(X)' must translate to -(all X p(X)) [got: $(grep -v '^%' "$L/T4a.out" | grep -m1 'p(X)' | tr -s ' ' | cut -c1-40)]"; }
c_T4b() { tcase T4b neg2.p    1 "~![X]:p(X) |- ?[X]:~p(X) must prove"; }
c_T4c() { tcase T4c negnest.p 1 "![X]:~?[Y]:q(X,Y) |- ~q(a,b) must prove (nested form)"; }
c_T4d() { tcase T4d neginv.p  0 "~![X]:p(X) |- p(c) must NOT prove (the bug makes it provable: unsound)"; }
c_T5()  { tcase T5  qn.p      0 "fof('EQ-var',...): translation must be readable by prover9"; }
c_T6a() { tcase T6a do.p      1 "distinct objects: \"Apple\" != \"Microsoft\" must prove"; }
c_T6b() { tcase T6b dc.p      0 "distinct object may equal an ordinary term: \"a\" != cat must NOT prove"; }
c_T7a() { x T7a 60 $VMEM "$C/sel.p" "$C" "$B/tptp_to_ladr"; has T7a 'label(i1)' && ! has T7a 'label(i2)'
          rec T7a $((! $?)) T7a "include('ax/M.ax',[i1,i3]) must omit i2"; }
c_T7b() { tcase T7b selq.p    0 "q(b) from the excluded formula i2 must NOT prove (the bug: unsound)"; }
c_T7c() { x T7c 60 $VMEM "$C/all.p" "$C" "$B/tptp_to_ladr"; [ "$(grep -c 'label(i[123])' "$L/T7c.out")" = 3 ]
          rec T7c $((! $?)) T7c "control: plain include/1 takes all 3"; }
c_T8a() { tcase T8a nand.p    1 "~& (NAND) must translate and mean -(a&b)"; }
c_T8b() { tcase T8b nor.p     1 "~| (NOR) must translate and mean -(a|b)"; }
c_AR()  { tcase AR  a300.p    1 "arity-300 predicate (old MAX_ARITY 255): must translate and prove"; }
# The same TPTP inputs through LADR-2026's own reader (prover9 file.p);
# n/a on builds without one.
nat() { # nat ID FILE WANT(1=prove,0=must-not) NOTE
  if [ "$NEW" != 1 ]; then na "$1" "no native TPTP reader in this build"; return; fi
  x "$1" 60 $VMEM /dev/null "$C" "$B/prover9" "$2"
  if [ "$3" = 1 ]; then o "$1" | grep -qE 'THEOREM PROVED|SZS status (Theorem|Unsatisfiable)'
  else [ "$RC" -lt 100 ] && ! o "$1" | grep -qE 'THEOREM PROVED|SZS status (Theorem|Unsatisfiable)|Fatal error'; fi
  rec "$1" $((! $?)) "$1" "native: $4"; }
c_T4n()  { nat T4n  neg2.p    1 "~![X]:p(X) |- ?[X]:~p(X) must prove"; }
c_T4cn() { nat T4cn negnest.p 1 "nested ![X]:~?[Y]:q |- ~q(a,b) must prove"; }
c_T4dn() { nat T4dn neginv.p  0 "~![X]:p(X) |- p(c) must NOT prove"; }
c_T5n()  { nat T5n  qn.p      0 "fof('EQ-var') read (no proof expected; must not crash)"; }
c_T6an() { nat T6an do.p      1 "distinct objects must prove"; }
c_T6bn() { nat T6bn dc.p      0 "\"a\" != cat must NOT prove"; }
c_T7bn() { nat T7bn selq.p    0 "selective include: q(b) must NOT prove"; }
c_T8an() { nat T8an nand.p    1 "NAND"; }
c_T8bn() { nat T8bn nor.p     1 "NOR"; }
c_ARn()  { nat ARn  a300.p    1 "arity 300"; }
# Mace4 tables and max_megs (M9 M10 M12), memory accounting (M11)
mt() {  # mt ID FILE MAXMEGS NOTE -- mace4 must refuse (exit 7) or stay within ~2x max_megs
  x "$1" 60 $VMEM /dev/null "$W" /usr/bin/time -f 'PEAK_RSS_KB=%M' "$B/mace4" -f "$C/$2"
  local rss; rss=$(o "$1" | sed -n 's/.*PEAK_RSS_KB=//p' | tail -1)
  [ "$RC" -lt 100 ] && ! has "$1" 'out of memory' && { [ "$RC" = 7 ] || [ "${rss:-0}" -le $(( $3 * 2048 )) ]; }
  rec "$1" $((! $?)) "$1" "$4 at max_megs $3 [peak RSS ${rss:-?} KB; $(o "$1" | grep -m1 -E 'would need|cell ids|too many cells|allocation of|negprop index|SIGSEGV|exit [(]' | cut -c1-80)]"; }
c_M9a() { mt M9a wide.in 500 "mace4 p/40: 2^40 cells, int total wraps"; }
c_M9b() { mt M9b big.in  500 "mace4 q/29: 2^29 cells = 30 GB table"; }
c_M10() { mt M10 np.in   20  "mace4 r/15: negprop index (d+1)^15 = ~344 MB"; }
c_M12() { x M12 120 $VMEM /dev/null "$W" /usr/bin/time -f 'PEAK_RSS_KB=%M' "$B/mace4" -n 10 -N 10 -f "$C/ground.in"
          local rss; rss=$(o M12 | sed -n 's/.*PEAK_RSS_KB=//p' | tail -1)
          [ "$RC" -lt 100 ] && ! has M12 'out of memory' && [ "${rss:-0}" -le 102400 ]
          rec M12 $((! $?)) M12 "mace4 -n 10, max_megs 20: ground-clause generation must stop at max_megs [peak RSS ${rss:-?} KB]"; }
c_M12c(){ x M12c 30 $VMEM /dev/null "$W" "$B/mace4" -f "$C/ok.in"
          has M12c 'Exiting with 1 model'; rec M12c $((! $?)) M12c "control: a feasible table still yields its model"; }
c_M11() { [ -s "$C/alg033.in" ] || { na M11 "needs TPTP ALG033+1 (not found)"; return; }
          x M11 "${M11_TIMEOUT:-120}" $VMEM /dev/null "$W" /usr/bin/time -f 'PEAK_RSS_KB=%M' "$B/prover9" -f "$C/megs500.in" "$C/alg033.in"
          local rss; rss=$(o M11 | sed -n 's/.*PEAK_RSS_KB=//p' | tail -1)
          local rep; rep=$(o M11 | grep -m1 -o 'Megabytes=[0-9.]*')
          # max_megs 500 must bound real memory: peak RSS <= 500 MB (512000 KB)
          [ "$RC" -lt 100 ] && [ "$RC" != 124 ] && [ "${rss:-0}" -le 512000 ]
          rec M11 $((! $?)) M11 "prover9 ALG033+1 at max_megs 500: peak RSS ${rss:-?} KB vs ${rep:-no Megabytes line}; $(o M11 | grep -m1 -oE 'exit \([a-z_]*\)|SZS status [A-Za-z]*')"; }
# Option dependencies (N1-N3) and in-range controls
opt() { x "$1" 30 $VMEM /dev/null "$W" "$B/prover9" -f "$C/$2.opt.in" "$C/triv.in"; }
c_N1a() { opt N1a n1_min;  ! has N1a 'Fatal error'; rec N1a $((! $?)) N1a "assign(max_minutes,-1) must be accepted"; }
c_N1b() { opt N1b n1_hrs;  ! has N1b 'Fatal error'; rec N1b $((! $?)) N1b "assign(max_hours,-1) must be accepted"; }
c_N1c() { opt N1c n1_days; ! has N1c 'Fatal error'; rec N1c $((! $?)) N1c "assign(max_days,-1) must be accepted"; }
c_N2a() { opt N2a n2_min;  ! has N2a 'Fatal error'; rec N2a $((! $?)) N2a "assign(max_minutes,50000000): int multiply overflow [$(o N2a | grep -m1 -E 'max_seconds|out of range' | tr -s ' ' | cut -c1-60)]"; }
c_N2b() { opt N2b n2_hrs;  ! has N2b 'Fatal error'; rec N2b $((! $?)) N2b "assign(max_hours,1000000): int multiply overflow [$(o N2b | grep -m1 -E 'max_seconds|out of range' | tr -s ' ' | cut -c1-60)]"; }
c_N3()  { opt N3 n3_pgr;   ! has N3 'Fatal error';  rec N3 $((! $?)) N3 "assign(pick_given_ratio,-1) must be accepted"; }
c_Nc()  { local r1 r2; opt Nc1 nc_min; has Nc1 'max_seconds, 120'; r1=$?
          opt Nc2 nc_pgr; has Nc2 'weight_part, 4'; r2=$?
          [ $r1 = 0 ] && [ $r2 = 0 ]; rec Nc $((! $?)) Nc2 "control: max_minutes 2 -> 120, pick_given_ratio 4 -> 4"; }
# UB visible only with UBSan (U1) -- in a plain build just check the run is normal
c_U1()  { x U1 60 $VMEM /dev/null "$W" "$B/prover9" -f "$C/x2.in"
          has U1 'THEOREM PROVED' && ! has U1 'runtime error'
          rec U1 $((! $?)) U1 "default prover9 run (x2.in); UBSan: signed overflow in init_giv_select [$(o U1 | grep -m1 -o 'giv_select.c:[0-9]*')]"; }
c_U2()  { x U2 120 $VMEM /dev/null "$W" "$B/prover9" -f "$C/u2_probe.in"
          ! has U2 'giv_select.c:[0-9]*:[0-9]*: runtime error'
          rec U2 $((! $?)) U2 "PROBE (no reproducer in FIXES.md): hints_part 2147483646 + sos_limit 20 [$(o U2 | grep -o 'giv_select.c:[0-9]*' | sort -u | tr '\n' ' ')]"; }
c_U3()  { x U3 120 $VMEM /dev/null "$W" "$B/prover9" -f "$C/u3_probe.in"
          ! has U3 'giv_select.c:[0-9]*:[0-9]*: runtime error'
          rec U3 $((! $?)) U3 "PROBE (no reproducer in FIXES.md): hints_part 2147483640, sos_limit 20 [$(o U3 | grep -o 'giv_select.c:[0-9]*' | sort -u | tr '\n' ' ')]"; }
# Sanitizer-only memory error (M7) and poison-only use-after-free (M8)
c_M7()  { x M7 60 $VMEM /dev/null "$W" "$B/prover9" -f "$C/prod.in"
          ! has M7 'AddressSanitizer' && [ "$RC" -lt 100 ]
          rec M7 $((! $?)) M7 "set(production): Op_codes heap overflow in init_dollar_eval [$(o M7 | grep -m1 -oE 'dollar.c:[0-9]+')]"; }
c_M7b() { x M7b 60 $VMEM /dev/null "$W" "$B/prover9" -f "$C/m7-production-overflow.in"
          ! has M7b 'AddressSanitizer' && [ "$RC" -lt 100 ]
          rec M7b $((! $?)) M7b "the old project's M7 demo input [$(o M7b | grep -m1 -oE 'dollar.c:[0-9]+')]"; }
c_M8()  { x M8 60 $VMEM /dev/null "$W" "$B/prover9" -f "$C/x2.in"
          has M8 'THEOREM PROVED'
          rec M8 $((! $?)) M8 "poisoned free_mem: default run on x2.in (zap_flatterm reads freed f->end)"; }
# Library-level reproducers (T9 PF5 M13): compile the test against ../ladr/libladr.a
unit() { # unit ID SRC NOTE
  local lib=$TREE/ladr/libladr.a
  [ -f "$lib" ] || { na "$1" "no $lib to link a unit test against"; return; }
  local extra=""; [ "$ASAN" = 1 ] && extra="-fsanitize=address,undefined"
  # The tests include "../ladr/x.h" (no -I: ladr/ has its own string.h that
  # would shadow libc's), so mirror that layout with a symlink in scratch.
  mkdir -p "$W/u/test"; ln -sfn "$TREE/ladr" "$W/u/ladr"; cp "$C/unit/$2" "$W/u/test/$2"
  gcc -g -O0 -w $extra -o "$W/$1.bin" "$W/u/test/$2" "$lib" -lm -lstdc++ > "$L/$1.compile" 2>&1 \
    || { na "$1" "unit test does not compile against this library (see $1.compile)"; return; }
  x "$1" 120 $VMEM /dev/null "$W" bash -c "ulimit -s 8192; exec '$W/$1.bin'"
  [ "$RC" = 0 ]; rec "$1" $((! $?)) "$1" "$3 [$(o "$1" | grep -E '^FAIL|failure|crashed' | head -3 | tr '\n' ' ' | cut -c1-110)]"; }
c_T9()  { unit T9  discrim_test_t9.c  "discrim index key in a short: symbol > 32767 must round-trip"; }
c_PF5() { unit PF5 pf5_lists.c     "10 recursive glist functions on 400,000-element lists, 8 MB stack"; }
c_M13() { unit M13 intpower_test.c "int_power must saturate, not wrap"; }
# Helper scripts (X1-X5)
c_X()   { local s bad=""
          for s in attack looper gvizify prover9-mace4; do
            head -1 "$B/$s" 2>/dev/null | grep -q '^#!/usr/bin/python$' && bad="$bad $s:python2-shebang"
            [ -x "$B/$s" ] || bad="$bad $s:not-executable"
          done
          cp "$C/head.in" "$W/head.in"
          x X1 90 $VMEM "$C/cands.txt" "$W" "$B/looper" mace4 assumptions head.in
          has X1 MODEL || bad="$bad looper-fails"
          rm -f "$W/interps"
          x X2 120 $VMEM "$C/cands.txt" "$W" "$B/attack" head.in interps
          { has X2 checked && [ -s "$W/interps" ]; } || bad="$bad attack-fails"
          x X5 60 $VMEM /dev/null "$W" "$B/gvizify" "$C/p.xml"
          has X5 'strict digraph' || bad="$bad gvizify-fails"
          x X3 90 $VMEM /dev/null "$W" "$B/prover9-mace4" -f "$C/x2.in"
          has X3 'THEOREM PROVED' || bad="$bad prover9-mace4-fails"
          [ -z "$bad" ]; rec X1-5 $((! $?)) X3 "helper scripts:${bad:- all four run}"; }

PLAIN="M1 M2 M2b M3a M3b M3c T1 M5 F1 D1a D1b D1c D1d D2a D2b T2a T2b T3
       TR T4a T4b T4c T4d T5 T6a T6b T7a T7b T7c T8a T8b AR
       T4n T4cn T4dn T5n T6an T6bn T7bn T8an T8bn ARn
       M9a M9b M10 M12 M12c M11 N1a N1b N1c N2a N2b N3 Nc U1 M7 T9 PF5 M13 X"
SAN="M7 M7b U1 N2a N2b T9 M13"   # U2/U3: no reproducer, see RESULTS.md (c_U2/c_U3 run on request)
SANONLY="M7 M7b U1 U2 U3"      # invisible without a sanitizer
POIONLY="M8"                   # invisible without the poisoned allocator
POI="M8"
if [ $# -gt 0 ]; then LIST="$*"
elif [ "$ASAN" = 1 ]; then LIST=$SAN
elif [ "$POISON" = 1 ]; then LIST=$POI
else LIST=$PLAIN; fi

echo "== $LABEL: $B  (asan=$ASAN poison=$POISON native-tptp=$NEW)"
for id in $LIST; do "c_$id"; done
echo "== summary: $SUM"
awk -F'\t' 'NR>1{n[$2]++} END{for (k in n) printf "   %s=%d", k, n[k]; print ""}' "$SUM"
