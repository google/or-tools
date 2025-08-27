%-----------------------------------------------------------------------------%
% Requires that the array 'x' is strictly lexicographically less than array 'y'.
% Compares them from first to last element, regardless of indices
%-----------------------------------------------------------------------------%

predicate ortools_lex_less_int(
  array [int] of var int: x,
  array [int] of var int: y,
);

predicate fzn_lex_less_int(
  array [int] of var int: x :: promise_ctx_antitone,
  array [int] of var int: y :: promise_ctx_monotone,
) = ortools_lex_less_int(x, y);