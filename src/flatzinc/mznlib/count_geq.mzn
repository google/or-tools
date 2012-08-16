%-----------------------------------------------------------------------------%
% Constrains 'c' to be greater or equal than or equal to the number of
% occurrences of % 'y' in 'x'.
%-----------------------------------------------------------------------------%

include "count_eq.mzn";

predicate count_geq(array[int] of var int: x, var int: y, var int: c) =
  let { var 0..length(x) : card }
  in count_eq(x, y, card) /\ c >= card;

%-----------------------------------------------------------------------------%
%-----------------------------------------------------------------------------%
