#!/bin/bash

#  Copyright (C) 2026 Chris Arena, 2026-10-16.
#  Written in collaboration with Claude Opus 5 (Anthropic).
#
#  NEW IN THIS TREE, AND NOT PART OF LADR-2009-11A: William McCune did
#  not write this file.  It was written for the audit of his release
#  that FIXES.md describes, and carries the library's licence so that
#  it can be distributed with it.
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
# promote.sh -- move rc onto master, but only when master's tree will be
#               identical to the tree the gates actually ran against.
#
# ---------------------------------------------------------------------------
# WHY "DID IT FAST-FORWARD" IS THE WRONG QUESTION
#
# The protocol here is: a feature branch is merged into rc, rc is gated, and
# rc becomes master.  The reason master can then skip the suite is NOT that
# the merge fast-forwarded -- it is that master's tree is byte-identical to
# the tree that passed.  Those two things come apart in both directions:
#
#   * A --no-ff merge of rc into master, with nothing else on master, gives
#     an identical tree.  The merge commit adds history, not content, and the
#     results transfer.  (Verified here: tree 606d0a38 on both sides, with
#     "git diff rc master" empty, across several such merges.)
#
#   * A merge that brings in a SECOND branch produces a tree that nothing was
#     ever gated against -- fast-forward or not.  Two features gated
#     separately and merged in sequence means the second merge's result was
#     never tested, however clean the history looks.
#
# So this script checks tree identity, and uses --ff-only for the promotion
# so that master ends up as literally the same commit as rc.  That makes the
# invariant structural instead of a thing to remember.
#
# ---------------------------------------------------------------------------
# USAGE
#
#   ./promote.sh GATED_COMMIT
#
# GATED_COMMIT is the commit the gates ran against -- in practice
# "git -C <the worktree you gated in> rev-parse HEAD".  The script refuses
# unless the only differences between it and rc are documentation, because a
# source change after the gate run means the gates tested something else.
# ---------------------------------------------------------------------------

set -u
gated=${1:?usage: promote.sh GATED_COMMIT   (git -C <gated worktree> rev-parse HEAD)}
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2

die() { echo "promote: $*" >&2; exit 1; }

git rev-parse --verify --quiet "$gated" >/dev/null || die "no such commit: $gated"
[ -z "$(git status --porcelain)" ] || die "working tree is dirty; commit or stash first"

# 1. The gated code must be the code on rc.  Documentation may differ: a
#    write-up committed after the gate run does not change what was tested.
src=$(git diff --name-only "$gated" rc | grep -vE '\.md$' || true)
if [ -n "$src" ]; then
  echo "promote: rc differs from the gated commit in source, so the gates" >&2
  echo "         did not test what you are about to promote:" >&2
  printf '           %s\n' $src >&2
  exit 1
fi
docs=$(git diff --name-only "$gated" rc | grep -cE '\.md$' || true)
echo "ok:   rc matches the gated commit ($docs documentation file(s) differ, no source)"

# 2. master must be an ancestor of rc, or this is not a promotion -- it is a
#    merge of divergent work, whose result nothing has gated.
if ! git merge-base --is-ancestor master rc; then
  echo "promote: master is not an ancestor of rc, so this would be a real" >&2
  echo "         merge and its result has not been gated.  Merge master into" >&2
  echo "         rc first, re-gate, then promote." >&2
  exit 1
fi
echo "ok:   master is an ancestor of rc, so the promotion is a fast-forward"

# 3. Promote.
before=$(git rev-parse master)
git checkout -q master || die "cannot check out master"
git merge --ff-only rc -q || die "--ff-only refused; see above"

# 4. And prove the invariant rather than trusting it.
if ! git diff --quiet rc master; then
  git checkout -q - 2>/dev/null
  die "master's tree differs from rc's after promotion -- this should be impossible"
fi
[ "$(git rev-parse master)" = "$(git rev-parse rc)" ] \
  || die "master is not the same commit as rc after --ff-only"

echo "ok:   master $(echo "$before" | cut -c1-9) -> $(git rev-parse --short master), same commit as rc"
echo "ok:   trees identical ($(git rev-parse master^{tree} | cut -c1-12)), so the suite need not re-run on master"
