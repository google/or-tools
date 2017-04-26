%-----------------------------------------------------------------------------%
% Constrains 'm' to be the minimum of the values in 'x'.
% Assumptions: |x| > 0.
%-----------------------------------------------------------------------------%

include "minimum_int.mzn";

predicate minimum(var int: m, array[int] of var int: x) =
    minimum_int(m, x);


