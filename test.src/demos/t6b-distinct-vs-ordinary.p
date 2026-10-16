% Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T6, the other direction -- a distinct object MAY equal an ordinary term.
%
% TPTP: distinct objects "are different from (but may be equal to) other
% tokens".  So "a" and the ordinary constant cat are not required to differ,
% and the conjecture below must NOT be provable.
%
% This demonstration exists to catch a plausible wrong fix.  A translator that
% made every distinct object unequal to everything -- rather than only to the
% other distinct objects -- would pass t6-distinct-objects.p and fail here.
% Without this file, the distinctness work would be only half tested.
%
% RUN IT
%   bin/tptp_to_ladr < test.src/demos/t6b-distinct-vs-ordinary.p > /tmp/t6b.in
%   bin/prover9 -f /tmp/t6b.in | grep -E 'THEOREM PROVED|SEARCH FAILED'
%
%   Expected on this tree:  SEARCH FAILED   (correct: it does not follow)
%------------------------------------------------------------------------------

fof(ax, axiom,
    p("a") & p("b") & p(cat) ).

fof(goal, conjecture,
    "a" != cat ).
