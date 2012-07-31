include "sliding_sum.mzn";

predicate sequence(array[int] of var int: x, int: k, int: lb, int: ub) =
    sliding_sum(lb, ub, k, x);
