predicate ortools_precede_chain_int(array [int] of int: T,
                                    array [int] of var int: X);

predicate fzn_value_precede_chain_int(array [int] of int: T,
                                      array [int] of var int: X) =
  if length(T) <= 1 then
    true
  elseif length(T) == 2 then
    fzn_value_precede_int(T[min(index_set(T))], T[max(index_set(T))], X)
  else
    ortools_precede_chain_int(T, X)
  endif;
 