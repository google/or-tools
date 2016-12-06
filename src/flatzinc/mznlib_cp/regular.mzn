%-----------------------------------------------------------------------------%
% The sequence of values in array 'x' (which must all be in the range 1..S)
% is accepted by the DFA of 'Q' states with input 1..S and transition
% function 'd' (which maps (1..Q, 1..S) -> 0..Q)) and initial state 'q0'
% (which must be in 1..Q) and accepting states 'F' (which all must be in
% 1..Q).  We reserve state 0 to be an always failing state.
%-----------------------------------------------------------------------------%
predicate regular(array[int] of var int: x, int: Q, int: S,
                  array[int,int] of int: d, int: q0, set of int: F);
