%-----------------------------------------------------------------------------%
% Requires that a set of tasks given by start times 's', durations 'd', and
% resource requirements 'r', never require more than a global resource bound
% 'b' at any one time.
% Assumptions:
% - forall i, d[i] >= 0 and r[i] >= 0
%-----------------------------------------------------------------------------%
predicate cumulative(array[int] of var int: s,
                     array[int] of var int: d,
                     array[int] of var int: r,
                     var int: b);
