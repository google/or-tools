%-----------------------------------------------------------------------------%
% Requires that the array 'x' is strictly lexicographically less than array 'y'.
% Compares them from first to last element, regardless of indices
%-----------------------------------------------------------------------------%

predicate lex_lesseq_int(array[int] of var int: x, array[int] of var int: y);

predicate lex_leq_int(array[int] of var int: x,
                      array[int] of var int: y) =
          lex_lesseq_int(x, y);

%-----------------------------------------------------------------------------%
