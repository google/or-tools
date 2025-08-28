predicate fzn_global_cardinality(
  array [int] of var int: x,
  array [int] of int: cover,
  array [int] of var int: counts,
) =
  forall (i in index_set(cover)) (count(x, cover[i], counts[i])) /\
    % Implied constraint
    length(x) >= sum(counts);