predicate ortools_global_cardinality(
  array [int] of var int: x,
  array [int] of int: cover,
  array [int] of var int: counts,
  bool: closed,
);

predicate fzn_global_cardinality(
  array [int] of var int: x,
  array [int] of int: cover,
  array [int] of var int: counts,
) = ortools_global_cardinality(x, cover, counts, false);
