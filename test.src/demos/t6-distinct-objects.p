% Copyright (C) 2026 Chris Arena, 2026-10-16.
% Written in collaboration with Claude Opus 5 (Anthropic).
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T6 -- TPTP's "distinct object" token could not be read at all.
%
% A distinct object is written in double quotes.  TPTP's own grammar notes
% (Documents/SyntaxBNF) define them:
%
%     <distinct_object>s are different from (but may be equal to) other
%     tokens, e.g., "cat" is different from 'cat' and cat.  Distinct objects
%     are always interpreted as themselves, so if they are different they are
%     unequal, e.g., "Apple" != "Microsoft" is implicit.
%
% Two things follow, and the second is easy to get wrong:
%   (a) distinct objects are PAIRWISE UNEQUAL, and nothing in the problem text
%       says so -- a reader has to supply it;
%   (b) a distinct object MAY still equal an ordinary term.  "cat" = cat is
%       permitted.  A translator that simply makes everything unequal is
%       wrong, which is why this demonstration checks both directions.
%
% WHAT THE UNFIXED TRANSLATOR DID
%   tptp_to_ladr calls set_quote_char('\''), so LADR's reader quotes with a
%   single quote and a double quote is not a lexable character at all.  Every
%   problem containing one died with
%
%       Fatal error:  sread_term error
%
%   That is 239 of the 1,060 Commonsense Reasoning problems, because
%   Axioms/CSR003+{0,1,2}.ax render Cyc's numeric literals this way -- "-1",
%   "0", "3.44" -- plus the BIO and SYO families.  For anyone comparing a
%   knowledge-base reasoner against Prover9 on commonsense problems, this one
%   defect removed most of the relevant corpus.
%
% WHY NOT MAP THEM TO NUMBERS
%   Tempting, since they are mostly numerals, and wrong twice over: LADR
%   cannot parse "3.44" at all, and prover9 does not prove 1 != 2 even with
%   set(eval_rewrite).  Numerals carry no built-in distinctness here.
%
% WHAT THE FIXED TRANSLATOR DOES
%   Each distinct object becomes a constant tptp_do_<n>, and their pairwise
%   inequality is stated explicitly as n(n-1)/2 disequalities.  The output
%   says how many it added.
%
%   A compact encoding was tried first and rejected: giving each object a bit
%   pattern over ceil(log2 n) fresh predicates needs 6,170 formulas instead of
%   190,036 for CSR003+2's 617 objects, and is a correct conservative
%   extension -- but it does not survive prover9's own default preprocessing.
%   With two objects the disequality is provable, with five it is not, and it
%   returns only under clear(auto).  A translator's output must work with the
%   default prover, so the expensive encoding is the right one.
%
%   The translator also emits clear(auto_denials) when it has added these
%   axioms.  They are negative unit clauses, and prover9's auto_denials
%   heuristic reads a Horn set with several negative clauses as a set of
%   candidate goals: it raised max_proofs to 10, proved the real goal, and
%   still reported SEARCH FAILED with "Proof 1 at 0.00 seconds" in the output.
%   The axioms are not goals, so it is told not to treat them as such.
%
% RUN IT
%   bin/tptp_to_ladr < test.src/demos/t6-distinct-objects.p > /tmp/t6.in
%   grep tptp_do /tmp/t6.in
%   bin/prover9 -f /tmp/t6.in | grep -E 'THEOREM PROVED|SEARCH FAILED'
%
%   Expected on this tree:  THEOREM PROVED
%   Expected on the 2009 code:  Fatal error: sread_term error
%
%   The companion check that distinctness has not been overdone is
%   t6b-distinct-vs-ordinary.p, where the goal must NOT be provable.
%------------------------------------------------------------------------------

fof(ax, axiom,
    p("Apple") & p("Microsoft") & p("Sun") ).

fof(goal, conjecture,
    "Apple" != "Microsoft" ).
