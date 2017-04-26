%-----------------------------------------------------------------------------%
% Requires that the array 'x' is strictly lexicographically less than array 'y'.
% Compares them from first to last element, regardless of indices
%-----------------------------------------------------------------------------%

predicate lex_lesseq_bool(array[int] of var bool: x, array[int] of var bool: y);

predicate lex_leq_bool(array[int] of var bool: x, array[int] of var bool: y) =
          lex_lesseq_bool(x, y);

%-----------------------------------------------------------------------------%
