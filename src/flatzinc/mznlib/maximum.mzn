%-----------------------------------------------------------------------------%
% Constrains 'm' to be the maximum of the values in 'x'.
% Assumptions: |x| > 0.
%-----------------------------------------------------------------------------%

predicate maximum_int(var int: m, array[int] of var int: x);

predicate maximum(var int: m, array[int] of var int: x) =
    maximum_int(m, x);

