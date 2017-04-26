%-----------------------------------------------------------------------------%
% A table constraint: table(x, t) represents the constraint x in t where we
% consider each row in t to be a tuple and t as a set of tuples.
%-----------------------------------------------------------------------------%

include "table_bool.mzn";
include "table_int.mzn";

predicate table(array[int] of var bool: x, array[int, int] of bool: t) =
    table_bool(x, t);

predicate table(array[int] of var int: x, array[int, int] of int: t) =
    table_int(x, t);
