predicate ortools_arg_max_int(array [int] of var int: x, var int: z, int: min_index, int: multiplier);

predicate fzn_minimum_arg_int(array [int] of var int: x, var int: z) =
  ortools_arg_max_int(x, z, min(index_set(x)), -1);
