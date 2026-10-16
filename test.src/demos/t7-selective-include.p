% Copyright (C) 2026 Chris Arena, 2026-10-16.
% Written in collaboration with Claude Opus 5 (Anthropic).
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T7 -- a selective include() ignored its name list and added axioms.
%
% TPTP's include has two forms:
%
%     include('Axioms/FOO.ax').               the whole file
%     include('Axioms/FOO.ax',[ia1,ia3]).     only the named formulas
%
% The second argument was ignored, so the whole file came in regardless.  That
% makes this a wrong-results defect of the same class as T4, not merely a
% missing feature: it quietly ADDS axioms the problem deliberately excluded,
% and extra axioms can make an unprovable conjecture provable.
%
% THIS DEMONSTRATION
%   The companion file ax/demo.ax holds three axioms:
%
%       i1:  p(a)
%       i2:  q(b)          <-- deliberately NOT selected below
%       i3:  r(c)
%
%   The include below names only i1 and i3.  The conjecture is q(b), which
%   follows from i2 alone.  So:
%
%     unfixed: i2 is pulled in anyway, and prover9 proves q(b)  -- WRONG,
%              the problem as written does not contain i2
%     fixed:   i2 is absent and prover9 cannot prove q(b)       -- correct
%
%   A demonstration that something must NOT be provable is the honest shape
%   here.  Checking only that i1 and i3 are present would pass on the unfixed
%   translator too, since it included everything.
%
% ONE CASE IS REFUSED RATHER THAN GUESSED
%   An include nested inside a selectively included file now raises an error.
%   TPTP's grammar does not settle whether the selection applies to the
%   formulas of the nested file, and resolving it either way would silently
%   change the axiom set -- which is the defect this fix is about.  Only two
%   problems in TPTP v9.3.1 use a selective include at all.
%
% RUN IT
%   cd test.src/demos
%   ../../bin/tptp_to_ladr < t7-selective-include.p > /tmp/t7.in
%   grep 'label(i' /tmp/t7.in              # expect i1 and i3, NOT i2
%   ../../bin/prover9 -f /tmp/t7.in | grep -E 'THEOREM PROVED|SEARCH FAILED'
%
%   Expected on this tree:  SEARCH FAILED  (q(b) does not follow)
%   Expected on the 2009 code:  THEOREM PROVED  (because i2 was added)
%------------------------------------------------------------------------------

include('ax/demo.ax',[i1,i3]).

fof(goal, conjecture,
    q(b) ).
