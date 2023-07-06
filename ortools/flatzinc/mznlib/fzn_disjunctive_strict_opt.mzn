predicate ortools_disjunctive_strict_opt(array [int] of var bool: o,
                                         array[int] of var int: s,
                                         array[int] of var int: d);

predicate fzn_disjunctive_strict_opt(array[int] of var opt int: s,
                                     array[int] of var int: d) = let {
    array[int] of var bool: os = [occurs(si) | si in s];
    array[int] of var int: ds = [if is_fixed(absent(si)) /\ fix(absent(si))
                                 then 0
                                 else deopt(si)
                                 endif | si in s];
} in ortools_disjunctive_strict_opt(os, ds, d);