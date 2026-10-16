% Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T8 -- TPTP's NAND and NOR were never declared to the parser.
%
% ladr/tptp_trans.c has had conversion branches for "~&" and "~|" since the
% translator was written, turning them into -(a & b) and -(a | b).  They were
% dead code: declare_tptp_input_types() never declared the operators, so the
% parser could not produce a "~&" or "~|" term and the branches were
% unreachable.  Any problem using either connective failed to translate.
%
% This is a quieter class of defect than T4: it fails loudly rather than
% returning a wrong answer.  It is included because of how it was found --
% not by reading the code, but by a semantic check of the translation.  Each
% TPTP form is translated and prover9 is then asked to prove the result
% equivalent to a hand-written LADR formula, in both directions.  A textual
% comparison would not have found this, because there was no output to
% compare: the translator simply refused the input.  That matrix lives in
% ../tptp_semantics.sh and went from 23/26 to 26/26 when these two operators
% were declared.
%
% The fix is two lines.  TPTP's grammar groups "~|" and "~&" with the other
% non-associative connectives (<=>, =>, <=, <~>), which is where <~> was
% already declared, so they take the same precedence.
%
% RUN IT
%   bin/tptp_to_ladr < test.src/demos/t8-nand-nor.p > /tmp/t8.in
%   bin/prover9 -f /tmp/t8.in | grep -E 'THEOREM PROVED|SEARCH FAILED'
%
%   Expected on this tree:  THEOREM PROVED
%   Expected on the 2009 code:  Fatal error (the operator does not parse)
%
% The conjecture restates the axiom with the connective spelled out, so it is
% provable exactly when "~&" and "~|" mean what TPTP says they mean.
%------------------------------------------------------------------------------

fof(nand_ax, axiom,
    p(a) ~& q(b) ).

fof(nor_ax, axiom,
    r(c) ~| s(d) ).

fof(goal, conjecture,
    ~ (p(a) & q(b)) & ~ (r(c) | s(d)) ).
