% Copyright (C) 2026 Chris Arena, 2026-10-16.
% Written in collaboration with Claude Opus 5 (Anthropic).
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T4 -- the negation of a quantified formula was silently dropped.
%
% This is the most serious defect found in LADR-2009-11A: tptp_to_ladr
% translated some problems into the OPPOSITE of what they said, with no
% warning and no error.  1,930 of the 17,912 first-order problems in TPTP
% v9.3.1 contain the shape that triggers it.
%
% WHAT TPTP SAYS
%   TPTP lets "~" apply to a quantified formula without parentheses, so the
%   axiom below says: it is NOT the case that p holds of everything.  That is,
%   something fails to be p.  The conjecture follows from it immediately.
%
% WHAT THE UNFIXED TRANSLATOR PRODUCED
%
%     formulas(assumptions).
%     p(X) # label(ax) # label(axiom).          <-- "everything IS p"
%     end_of_list.
%
%   Both the negation and the quantifier vanished, leaving the body with a
%   free variable, which LADR reads as universally quantified.  The axiom was
%   turned into its own contrary.  prover9 then reports SEARCH FAILED on this
%   file, because the conjecture genuinely does not follow from "everything is
%   p".
%
% WHAT THE FIXED TRANSLATOR PRODUCES
%
%     formulas(assumptions).
%     -(all X p(X)) # label(ax) # label(axiom).
%     end_of_list.
%
%   and prover9 reports THEOREM PROVED.
%
% WHY IT HAPPENED
%   ladr/tptp_trans.c declares "~" at precedence 410 and ":" at 450, so "~"
%   binds tighter and the parser produced
%
%       :( ~(!([X])) , p(X) )      instead of     ~( :( !([X]), p(X) ) )
%
%   -- the negation attached to the quantifier PREFIX rather than to the
%   quantified formula.  The converter's ":" branch, whose comment read
%   "Assume it's well formed (don't check for errors)", then called
%   listterm_reverse() on "!([X])", which is not a list, got the empty list
%   back, never entered its variable loop, and returned the body.
%
%   Precedence alone cannot fix this.  Moving "~" outside ":" makes
%   "! [X] : ~ p(X)" fail instead, because ":" can then no longer take a
%   "~"-formula as its right operand: the failures swap and the count stays at
%   four.  TPTP's grammar permits both nestings because "~ <unitary>" and
%   "! [X] : <unitary>" recurse through the same nonterminal, which one
%   precedence per operator cannot express.  The Changelog records McCune
%   trying twice, one attempt noted as "no good", and his "changed by McCune"
%   comment is still on the line.  The fix is in the converter: peel any "~"
%   the parser bound to the quantifier prefix and re-apply it outside.
%
% HOW IT WAS FOUND
%   As a false accusation against Mace4.  A sweep of TPTP flagged dozens of
%   problems where Mace4 reported a model for a problem TPTP declares a
%   Theorem, which would be a soundness bug in the model finder.  It was not:
%   the translator had inverted an axiom and Mace4 was correctly modelling a
%   different problem.  Every apparent soundness violation traced back here.
%
% RUN IT
%   bin/tptp_to_ladr < test.src/demos/t4-negated-quantifier.p
%   bin/tptp_to_ladr < test.src/demos/t4-negated-quantifier.p > /tmp/t4.in
%   bin/prover9 -f /tmp/t4.in | grep -E 'THEOREM PROVED|SEARCH FAILED'
%
%   Expected on this tree:  THEOREM PROVED
%   Expected on the 2009 code:  SEARCH FAILED
%------------------------------------------------------------------------------

fof(ax, axiom,
    ~ ! [X] : p(X) ).

fof(goal, conjecture,
    ? [X] : ~ p(X) ).
