predicate ortools_cumulative_opt(array [int] of var bool: o,
                                 array[int] of var int: s,
                                 array[int] of var int: d,
                                 array[int] of var int: r,
                                 var int: b);

predicate fzn_cumulative_opt(array[int] of var opt int: s,
                             array[int] of var int: d,
                             array[int] of var int: r,
                             var int: b) = let {
    array[int] of var bool: os = [occurs(si) | si in s];
    array[int] of var int: ds = [if is_fixed(absent(si)) /\ fix(absent(si))
                                 then 0
                                 else deopt(si)
                                 endif | si in s];
} in ortools_cumulative_opt(os, ds, d, r, b);