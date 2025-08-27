%-----------------------------------------------------------------------------%
% Requires that the array 'x' is strictly lexicographically less than array 'y'.
% Compares them from first to last element, regardless of indices
%-----------------------------------------------------------------------------%

predicate ortools_lex_less_bool(
  array [int] of var bool: x ,
  array [int] of var bool: y,
);

predicate fzn_lex_less_bool(
  array [int] of var bool: x :: promise_ctx_antitone,
  array [int] of var bool: y :: promise_ctx_monotone,
) = ortools_lex_less_bool(x, y);