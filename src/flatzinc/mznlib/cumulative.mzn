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

predicate variable_cumulative(array[int] of var int: s,
                              array[int] of var int: d,
                              array[int] of var int: r,
                              var int: b) =
    assert(index_set(s) == index_set(d) /\ index_set(s) == index_set(r),
        "cumulative: the 3 array arguments must have identical index sets",
        assert(lb_array(d) >= 0 /\ lb_array(r) >= 0,
            "cumulative: durations and resource usages must be non-negative",
            let {
               set of int: tasks =
                  {i | i in index_set(s) where ub(r[i]) > 0 /\ ub(d[i]) > 0 },
               set of int: times =
                  min([ lb(s[i]) | i in tasks ]) ..
                  max([ ub(s[i]) + ub(d[i]) | i in tasks ])
                }
            in
               forall( t in times ) (
                  b >= sum( i in tasks ) (
                     bool2int( s[i] <= t /\ t < s[i] + d[i] ) * r[i]
                  )
               )
        )
    );

predicate cumulative(array[int] of var int: s,
                     array[int] of int: d,
                     array[int] of int: r,
                     int: b) =
    fixed_cumulative(s, d, r, b);

predicate cumulative(array[int] of var int: s,
                     array[int] of var int: d,
                     array[int] of var int: r,
                     var int: b) =
    variable_cumulative(s, d, r, b);

