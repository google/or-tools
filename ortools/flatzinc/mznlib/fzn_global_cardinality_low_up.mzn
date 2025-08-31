predicate ortools_global_cardinality_low_up(
  array [int] of var int: x,
  array [int] of int: cover,
  array [int] of int: lbound,
  array [int] of int: ubound,
  bool: closed,
);

predicate fzn_global_cardinality_low_up(
  array [int] of var int: x,
  array [int] of int: cover,
  array [int] of int: lbound,
  array [int] of int: ubound,
) = ortools_global_cardinality_low_up(x, cover, lbound, ubound, false);
