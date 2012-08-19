%-----------------------------------------------------------------------------%
% Constrains 'c' to be the number of occurrences of 'y' in 'x'.
%-----------------------------------------------------------------------------%

include "count_reif.mzn";

predicate count_eq_reif(array[int] of var int: x, var int: y, var int: c,
                        var bool: b)
    = count_reif(x, y, c, b);

predicate count_eq(array[int] of var int: x, var int: y, var int: c);

%-----------------------------------------------------------------------------%
%-----------------------------------------------------------------------------%
