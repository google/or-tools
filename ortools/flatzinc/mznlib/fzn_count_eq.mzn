predicate ortools_count_eq(array [int] of var int: x, var int: y, var int: c);
predicate ortools_count_eq_cst(array [int] of var int: x, int: y, var int: c);

predicate fzn_count_eq(array [int] of var int: x, int: y, var int: c) =
    ortools_count_eq_cst(x, y, c);

predicate fzn_count_eq(array [int] of var int: x, var int: y, var int: c) =
    ortools_count_eq(x, y, c);
