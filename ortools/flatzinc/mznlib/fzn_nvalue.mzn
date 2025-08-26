predicate ortools_nvalue(var int:n, array[int] of var int: x);

predicate fzn_nvalue(var int: n,  array [int] of var int: x) =
    ortools_nvalue(n, x);
