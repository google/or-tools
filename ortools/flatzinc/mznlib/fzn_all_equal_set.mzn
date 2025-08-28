predicate fzn_all_equal_set(array [int] of var set of int: x) =
  if length(x) > 1 then
    forall (i in index_set(x) diff {min(index_set(x))}) (x[min(index_set(x))] = x[i])
  endif;