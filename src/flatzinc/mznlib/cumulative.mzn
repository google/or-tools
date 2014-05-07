%-----------------------------------------------------------------------------%
% Requires that a set of tasks given by start times 's', durations 'd', and
% resource requirements 'r', never require more than a global resource bound
% 'b' at any one time.
% Assumptions:
% - forall i, d[i] >= 0 and r[i] >= 0
%-----------------------------------------------------------------------------%
predicate fixed_cumulative(array[int] of var int: s,
                           array[int] of int: d,
                           array[int] of int: r,
                           int: b);

predicate var_cumulative(array[int] of var int: s,
                         array[int] of int: d,
                         array[int] of int: r,
                         var int: b);

predicate variable_cumulative(array[int] of var int: s,
                              array[int] of var int: d,
                              array[int] of var int: r,
                              var int: b);

predicate cumulative(array[int] of var int: s,
                     array[int] of int: d,
                     array[int] of int: r,
                     int: b) =
    fixed_cumulative(s, d, r, b);

predicate cumulative(array[int] of var int: s,
                     array[int] of int: d,
                     array[int] of int: r,
                     var int: b) =
    var_cumulative(s, d, r, b);

predicate cumulative(array[int] of var int: s,
                     array[int] of var int: d,
                     array[int] of var int: r,
                     var int: b) =
    variable_cumulative(s, d, r, b);

