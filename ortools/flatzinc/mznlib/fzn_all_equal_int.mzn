predicate fzn_all_equal_int(array [int] of var int: x) =
  if length(x) > 1 then
    forall (i in index_set(x) diff {min(index_set(x))}) (x[min(index_set(x))] = x[i])
  endif;
