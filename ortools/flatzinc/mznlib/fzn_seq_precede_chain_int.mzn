predicate ortools_precede_chain_int(array [int] of int: T,
                                    array [int] of var int: X);

predicate fzn_seq_precede_chain_int(array [int] of var int: X) =
  ortools_precede_chain_int(1..max(ub_array(X), 1), X);
 