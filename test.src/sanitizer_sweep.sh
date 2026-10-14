#!/bin/bash
#------------------------------------------------------------------------------
# The outer gate: build with AddressSanitizer and UndefinedBehaviorSanitizer,
# verify the build really is instrumented, then run the whole corpus through
# both provers and report every sanitizer complaint.
#
# WHY THIS IS A SCRIPT NOW
#   test.src/Makefile said the sanitizer sweep "belongs at the merge-to-rc and
#   release gates", but it was not a make target and not a script -- it was
#   done by hand.  It was then skipped for five consecutive merges to rc and
#   master, during work that rewrote forty list functions as manual pointer
#   surgery, changed the layout of struct di_tree, and introduced new
#   malloc/free paths.  Exactly the changes the sweep exists to check.
#
#   A gate that depends on remembering to run it is not a gate.
#
# WHY IT RUNS IN PARALLEL
#   The first version of this script was a serial "while read" loop.  On an
#   80-core machine that leaves 79 cores idle, and the symptom is deceptive:
#   `top` shows prover9 at about 1.2% CPU, which looks like something is
#   stalled when in fact the process is pegged at 100% of its one core and
#   1.25% is simply one core out of eighty.  Thirty minutes in, the sweep had
#   hours left.
#
#   Parallelism is safe here for the same reason it was safe for the capability
#   sweep: the per-run budget is `ulimit -t`, which is CPU time, so it means
#   the same thing whether one run or sixty are in flight.  A wall-clock guard
#   would not be -- an earlier run of this project had 39% of its "timeouts"
#   killed by a wall guard under load rather than by the real limit.
#
# WHY THE INSTRUMENTATION IS VERIFIED TWICE
#   An earlier version of this check looked for __asan_init in the binary and
#   concluded the build was instrumented.  That is worthless: linking the ASan
#   runtime says nothing about which OBJECTS carry the flags, and in the build
#   that prompted the lesson, libladr was compiled without them while
#   provers.src was compiled with them.  The positive control used a defect in
#   provers.src and so passed, and a whole sweep was believed on that basis.
#   See the "Correction" entry in ../FIXES.md.
#
#   So this checks two things that cannot both be true of a half-instrumented
#   build: that every compile line carried -fsanitize, and that every member of
#   libladr.a references __asan.
#
# USAGE
#   sanitizer_sweep.sh CORPUS_LIST [BUILD_DIR] [JOBS]
#     CORPUS_LIST is a file of input paths, one per line.
#     JOBS defaults to every core, capped by available memory.
#
#   Exit status 0 when the build verifies and no run reports a sanitizer error.
#------------------------------------------------------------------------------

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
corpus="${1:-}"
build="${2:-${TMPDIR:-/tmp}/ladr-sanitize-$$}"
# Default to EVERY core.  On this hardware the only real constraint is
# physical memory, not core count: a serial version of this sweep left 79 of
# 80 cores idle and turned 20 minutes of work into hours, and the symptom was
# deceptive -- prover9 showing ~1.2% CPU looks stalled when in fact it is
# pegged at 100% of its one core and 1.25% is one core out of eighty.
#
# The cap is memory: an ASan run carries shadow memory as well as max_megs,
# so budget 1500 MB per concurrent run against MemAvailable.
mem_mb=$(awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo)
mem_cap=$(( mem_mb / 1500 ))
[ "$mem_cap" -lt 1 ] && mem_cap=1
cores=$(nproc)
if [ "$mem_cap" -lt "$cores" ]; then default_jobs="$mem_cap"; else default_jobs="$cores"; fi
jobs="${3:-$default_jobs}"
[ "$jobs" -lt 1 ] && jobs=1

[ -n "$corpus" ] && [ -r "$corpus" ] || {
  echo "usage: $0 CORPUS_LIST [BUILD_DIR] [JOBS]" >&2; exit 2; }

echo "=== building an instrumented tree in $build ==="
rm -rf "$build"
mkdir -p "$build"
# Sources only, from git, so no stale object files can be linked in.
( cd "$root" && git archive HEAD ) | tar -x -C "$build" || {
  echo "could not export sources from git" >&2; exit 2; }

SAN='-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1'
( cd "$build" && make XFLAGS="$SAN" all ) > "$build/build.log" 2>&1
rc=$?
if [ "$rc" != 0 ] || [ ! -x "$build/bin/prover9" ]; then
  echo "BUILD FAILED (see $build/build.log)" >&2
  tail -20 "$build/build.log" >&2
  exit 2
fi

echo
echo "=== verifying the instrumentation, two ways ==="

# Both patterns must be order-independent: -fsanitize comes from XFLAGS and so
# appears BEFORE -c on the line.  An earlier version required it after, found
# 0 of 137, and refused a build that was in fact fully instrumented --
# fail-closed, but it would have hidden the sweep behind a false negative.
compile_re='(^|[[:space:]])(gcc|cc|g\+\+)[[:space:]].*[[:space:]]-c([[:space:]]|$)'
total=$(grep -acE "$compile_re" "$build/build.log")
sanlines=$(grep -aE "$compile_re" "$build/build.log" | grep -ac -- -fsanitize)
printf '  compile lines carrying -fsanitize: %s of %s\n' "$sanlines" "$total"
[ "$total" -gt 0 ] && [ "$sanlines" = "$total" ] || {
  echo "  NOT FULLY INSTRUMENTED -- refusing to report a sweep on this build" >&2
  exit 2; }

objs=$(ar t "$build/ladr/libladr.a" | wc -l)
withasan=0
for o in $(ar t "$build/ladr/libladr.a"); do
  ( cd "$build/ladr" && ar p libladr.a "$o" > "$build/obj.tmp" ) 2>/dev/null
  nm "$build/obj.tmp" 2>/dev/null | grep -q __asan && withasan=$((withasan + 1))
done
rm -f "$build/obj.tmp"
printf '  libladr.a objects referencing __asan: %s of %s\n' "$withasan" "$objs"
[ "$objs" -gt 0 ] && [ "$withasan" = "$objs" ] || {
  echo "  NOT FULLY INSTRUMENTED -- refusing to report a sweep on this build" >&2
  exit 2; }

echo
echo "=== sweeping the corpus, $jobs at a time ==="
out="$build/sweep"
mkdir -p "$out"

# One run per problem per prover, as its own process so xargs can fan out.
# A clean log is deleted; anything left behind is a finding.
cat > "$build/worker.sh" <<'WORKER'
#!/bin/bash
build="$1"; f="$2"
# halt_on_error=0 so a run reports everything it finds rather than the first
# thing; detect_leaks=0 because LADR frees through its own arena and never
# returns it, which is a design choice and not the defect class being hunted.
export ASAN_OPTIONS="halt_on_error=0:detect_leaks=0:abort_on_error=0"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"
if [ ! -r "$f" ]; then echo "UNREADABLE|$f"; exit 0; fi
for prog in prover9 mace4; do
  log="$build/sweep/$(basename "$f").$prog.log"
  ( ulimit -t 60; "$build/bin/$prog" -f "$f" ) > /dev/null 2> "$log"
  if grep -qaE 'ERROR: AddressSanitizer|runtime error:|LeakSanitizer' "$log"; then
    echo "REPORT|$prog|$f"
  else
    rm -f "$log"
  fi
done
WORKER
chmod +x "$build/worker.sh"

xargs -a "$corpus" -P "$jobs" -I{} "$build/worker.sh" "$build" "{}" \
  > "$build/findings.txt" 2>/dev/null

runs=$(( $(grep -cve '^[[:space:]]*$' "$corpus") * 2 ))
n=$(grep -ac '^REPORT|' "$build/findings.txt" || true)
u=$(grep -ac '^UNREADABLE|' "$build/findings.txt" || true)

while IFS='|' read -r kind a b; do
  case "$kind" in
    REPORT) echo "  SANITIZER REPORT: $a $(basename "$b")"
            grep -aE 'ERROR: AddressSanitizer|runtime error:' \
              "$out/$(basename "$b").$a.log" 2>/dev/null | head -3 | sed 's/^/      /' ;;
    UNREADABLE) echo "  UNREADABLE: $a" ;;
  esac
done < "$build/findings.txt"

echo
printf 'ran %s prover/problem pairs; %s produced a sanitizer report' "$runs" "$n"
[ "$u" != 0 ] && printf ', %s unreadable' "$u"
echo
if [ "$n" = 0 ] && [ "$u" = 0 ]; then
  echo "sanitizer_sweep: CLEAN"
  rm -rf "$build"
  exit 0
else
  echo "sanitizer_sweep: $n report(s), $u unreadable -- logs kept in $out"
  exit 1
fi
