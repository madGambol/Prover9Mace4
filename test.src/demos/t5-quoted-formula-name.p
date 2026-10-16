% Copyright (C) 2026 Chris Arena and Claude Opus 5, 2026-10-16.
% GPL-2.0-only, the same terms as the tree this exercises;
% see COPYING at the top of it.  Not part of LADR-2009-11A.
%------------------------------------------------------------------------------
% T5 -- the translator emitted files its own reader rejects.
%
% A TPTP formula name may be a single-quoted atom.  TPTP quotes with ' and
% LADR quotes with ", and the name was copied straight through into the LADR
% label:
%
%     fof('EQ-var', axiom, ...)   ->   ... # label('EQ-var') # label(axiom).
%
% LADR's reader then rejects the file the translator has just written:
%
%     Fatal error:  sread_term error
%     %%ERROR: A term cannot be constructed from the marked string:
%     ... # label(%%START ERROR%%'EQ-var'%%END ERROR%%) # label(axiom).
%
% 382 of the 17,912 first-order problems in TPTP v9.3.1 carry names of this
% form.  Every one of them failed at the PROVER rather than at the translator,
% so nothing in the translation output hinted at the cause -- the translator
% exited 0 and looked successful.
%
% The -q option does not help: it remaps function and predicate symbols that
% are illegal in LADR, not formula names.
%
% The fix re-quotes such a name in LADR's style, leaving names that are
% already acceptable untouched.
%
% RUN IT
%   bin/tptp_to_ladr < test.src/demos/t5-quoted-formula-name.p > /tmp/t5.in
%   grep label /tmp/t5.in                    # expect "EQ-var", not 'EQ-var'
%   bin/prover9 -f /tmp/t5.in 2>&1 | grep -c 'sread_term error'
%
%   Expected on this tree:  0 read errors
%   Expected on the 2009 code:  2 read errors, and no search at all
%------------------------------------------------------------------------------

fof('EQ-var', axiom,
    p(a) ).

fof('goal-with-a-hyphen', conjecture,
    p(a) ).
